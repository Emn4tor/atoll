/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "aitools.h"

#include "ai/commandrunner.h"
#include "ai/screencapture.h"
#include "config/config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
/** A JSON Schema string property, with its description. */
QJsonObject str(const QString &description)
{
    return QJsonObject{{u"type"_s, u"string"_s}, {u"description"_s, description}};
}

QJsonObject stringArray(const QString &description)
{
    return QJsonObject{{u"type"_s, u"array"_s},
                       {u"description"_s, description},
                       {u"items"_s, QJsonObject{{u"type"_s, u"string"_s}}}};
}

QJsonObject tool(const QString &name,
                 const QString &description,
                 const QJsonObject &properties,
                 const QStringList &required)
{
    QJsonArray requiredArray;
    for (const QString &key : required) {
        requiredArray.append(key);
    }
    return QJsonObject{{u"name"_s, name},
                       {u"description"_s, description},
                       {u"input_schema"_s,
                        QJsonObject{{u"type"_s, u"object"_s},
                                    {u"properties"_s, properties},
                                    {u"required"_s, requiredArray}}}};
}

/** The first package manager on this machine, and how it is driven. */
struct PackageManager {
    QString program;
    QStringList install;
    QStringList update;
};

PackageManager detectPackageManager()
{
    struct Candidate {
        const char *program;
        QStringList install;
        QStringList update;
    };
    // AUR helpers first: on Arch they are a superset of pacman, and a user who
    // has one installed expects it to be what answers "install ...".
    static const QList<Candidate> candidates = {
        {"paru", {u"-S"_s, u"--noconfirm"_s, u"--needed"_s}, {u"-Syu"_s, u"--noconfirm"_s}},
        {"yay", {u"-S"_s, u"--noconfirm"_s, u"--needed"_s}, {u"-Syu"_s, u"--noconfirm"_s}},
        {"pacman", {u"-S"_s, u"--noconfirm"_s, u"--needed"_s}, {u"-Syu"_s, u"--noconfirm"_s}},
        {"apt-get", {u"install"_s, u"-y"_s}, {u"update"_s}},
        {"dnf", {u"install"_s, u"-y"_s}, {u"upgrade"_s, u"-y"_s}},
        {"zypper", {u"--non-interactive"_s, u"install"_s}, {u"--non-interactive"_s, u"dup"_s}},
    };
    for (const Candidate &candidate : candidates) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(candidate.program));
        if (!path.isEmpty()) {
            return {QString::fromLatin1(candidate.program), candidate.install, candidate.update};
        }
    }
    return {};
}

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(u"'"_s, u"'\\''"_s);
    return u"'"_s + escaped + u"'"_s;
}

QString expandPath(const QString &path)
{
    if (path.startsWith(u'~')) {
        return QDir::cleanPath(QDir::homePath() + path.mid(1));
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}
}

AiToolbox::AiToolbox(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_runner(new CommandRunner(this))
    , m_capture(new ScreenCapture(this))
{
    connect(m_runner, &CommandRunner::progress, this, &AiToolbox::progress);
    connect(m_runner, &CommandRunner::finished, this,
            [this](const QString &token, int exitCode, const QString &output, bool timedOut) {
                QString body = output.isEmpty() ? u"(no output)"_s : output;
                if (timedOut) {
                    body.prepend(u"The command was still running after the time limit and was stopped.\n"_s);
                } else if (exitCode != 0) {
                    body.prepend(u"Exit status %1.\n"_s.arg(exitCode));
                }
                finish(token, body, exitCode != 0);
            });

    connect(m_capture, &ScreenCapture::captured, this, [this](const QByteArray &png) {
        AiToolResult result;
        result.id = m_pending;
        result.content = u"Here is the current screen."_s;
        result.image = png;
        result.imageMediaType = u"image/png"_s;
        m_pending.clear();
        Q_EMIT completed(result);
    });
    connect(m_capture, &ScreenCapture::failed, this, [this](const QString &reason) {
        finish(m_pending, reason, true);
    });
}

QString AiToolbox::systemPrompt(const QString &userAddition)
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP", u"KDE"_s);
    const PackageManager manager = detectPackageManager();

    QString prompt = uR"(You are the assistant built into Atoll, a dynamic island for the KDE Plasma
desktop. You are talking to someone using their own computer, often someone who
is new to Linux. Your answers appear in a small panel at the top of the screen,
so keep them short: two or three sentences unless the question genuinely needs
more. No markdown headings, no bullet lists longer than four items.

You can act on this machine through the tools you have been given. Prefer a
named tool over run_command when one fits, because the named tool is what the
user is shown before they approve it.

Rules that matter:

- Ask for the least you need. Never reach for administrator rights when the work
  fits inside the user's own account; the system will refuse you anyway and the
  user will see that you asked.
- Before something irreversible, say what you are about to do in one line.
- If a command fails, read the error and try one sensible alternative. Do not
  loop on the same failing command.
- When you install or change something, say afterwards what changed, in plain
  words. Assume the person does not know what a daemon is.
- If a request is unclear, ask one short question instead of guessing.
- You cannot see the screen unless you take a screenshot or the user attached
  one. Do not pretend to.

Machine details:
- Distribution: %1
- Desktop: %2
- Package manager: %3
- Home directory: %4
)"_s.arg(QSysInfo::prettyProductName(),
         desktop,
         manager.program.isEmpty() ? u"none detected"_s : manager.program,
         QDir::homePath());

    if (!userAddition.trimmed().isEmpty()) {
        prompt += u"\nThe user added these standing instructions:\n"_s + userAddition.trimmed() + u"\n"_s;
    }

    // Anything the assistant was asked to remember comes back as context, not
    // as an instruction it has to be told to look for.
    const QString memoryPath = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                                   .filePath(u"atoll/assistant-memory.md"_s);
    QFile memory(memoryPath);
    if (memory.open(QIODevice::ReadOnly)) {
        const QString text = QString::fromUtf8(memory.read(8000)).trimmed();
        if (!text.isEmpty()) {
            prompt += u"\nThings you were asked to remember about this user:\n"_s + text + u"\n"_s;
        }
    }

    return prompt;
}

QJsonArray AiToolbox::definitions(bool screenshotAllowed)
{
    QJsonArray tools;

    tools.append(tool(u"run_command"_s,
                      u"Run a shell command on this machine and read what it printed. Use this for "
                      "anything the named tools do not cover. Write the command as you would type it "
                      "in a terminal. Do not prefix it with sudo: say what you need in `purpose` and "
                      "the system decides whether administrator rights apply and prompts the user "
                      "itself."_s,
                      QJsonObject{{u"command"_s, str(u"The command line to run."_s)},
                                  {u"purpose"_s,
                                   str(u"One short sentence, in plain language, describing what this "
                                       "does. It is shown to the user when they are asked to approve it."_s)}},
                      {u"command"_s, u"purpose"_s}));

    tools.append(tool(u"read_file"_s,
                      u"Read a text file."_s,
                      QJsonObject{{u"path"_s, str(u"Absolute path, or one starting with ~."_s)}},
                      {u"path"_s}));

    tools.append(tool(u"write_file"_s,
                      u"Create or replace a text file with the exact contents given. Read the file "
                      "first if you are editing rather than creating it."_s,
                      QJsonObject{{u"path"_s, str(u"Absolute path, or one starting with ~."_s)},
                                  {u"content"_s, str(u"The complete new contents of the file."_s)}},
                      {u"path"_s, u"content"_s}));

    tools.append(tool(u"list_directory"_s,
                      u"List what is in a directory."_s,
                      QJsonObject{{u"path"_s, str(u"Absolute path, or one starting with ~."_s)}},
                      {u"path"_s}));

    tools.append(tool(u"install_packages"_s,
                      u"Install software with the system package manager. Search first with "
                      "run_command if you are not sure of the exact package name."_s,
                      QJsonObject{{u"packages"_s, stringArray(u"Exact package names."_s)},
                                  {u"reason"_s, str(u"One line on why, shown to the user."_s)}},
                      {u"packages"_s}));

    tools.append(tool(u"system_update"_s,
                      u"Refresh the package databases and upgrade everything installed."_s,
                      QJsonObject{},
                      {}));

    tools.append(tool(u"set_wallpaper"_s,
                      u"Set the desktop wallpaper to an image file that already exists on disk."_s,
                      QJsonObject{{u"path"_s, str(u"Absolute path to an image."_s)}},
                      {u"path"_s}));

    tools.append(tool(u"plasma_setting"_s,
                      u"Change one KDE Plasma configuration value. This is the reliable way to alter "
                      "the desktop: it edits the same files System Settings does."_s,
                      QJsonObject{{u"file"_s, str(u"Config file name, e.g. kdeglobals or kwinrc."_s)},
                                  {u"group"_s,
                                   str(u"Group path, e.g. \"General\" or \"Effect-blur\". Separate nested "
                                       "groups with a forward slash."_s)},
                                  {u"key"_s, str(u"The key to set."_s)},
                                  {u"value"_s, str(u"The new value."_s)}},
                      {u"file"_s, u"group"_s, u"key"_s, u"value"_s}));

    tools.append(tool(u"launch_app"_s,
                      u"Open an application or a file."_s,
                      QJsonObject{{u"target"_s,
                                   str(u"A .desktop entry name, an executable, a file path or a URL."_s)}},
                      {u"target"_s}));

    if (screenshotAllowed) {
        tools.append(tool(u"take_screenshot"_s,
                          u"Take one picture of the screen and look at it. Use this when the question "
                          "is about something the user can see."_s,
                          QJsonObject{},
                          {}));
    }

    tools.append(tool(u"notify"_s,
                      u"Show a short message on the island. Use it to report that a long job in the "
                      "background has finished."_s,
                      QJsonObject{{u"summary"_s, str(u"A few words."_s)},
                                  {u"body"_s, str(u"One optional line of detail."_s)}},
                      {u"summary"_s}));

    tools.append(tool(u"remember"_s,
                      u"Store one lasting fact about this user or their machine, so later "
                      "conversations start from it. Keep it to a single line."_s,
                      QJsonObject{{u"fact"_s, str(u"The thing worth remembering."_s)}},
                      {u"fact"_s}));

    return tools;
}

void AiToolbox::finish(const QString &id, const QString &content, bool isError)
{
    if (id.isEmpty()) {
        return;
    }
    if (m_pending == id) {
        m_pending.clear();
    }
    AiToolResult result;
    result.id = id;
    result.content = content;
    result.isError = isError;
    Q_EMIT completed(result);
}

void AiToolbox::runShell(const QString &id, const QString &command, bool elevated)
{
    m_pending = id;
    const int timeout = m_config->value(u"ai.commandTimeout"_s, 180).toInt() * 1000;
    m_runner->run(id, command, elevated, timeout);
}

QString AiToolbox::readFile(const QVariantMap &input) const
{
    const QString path = expandPath(input.value(u"path"_s).toString());
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return u"Cannot read %1: %2"_s.arg(path, file.errorString());
    }
    const QByteArray bytes = file.read(200000);
    return CommandRunner::condense(QString::fromUtf8(bytes));
}

QString AiToolbox::writeFile(const QString &id, const QVariantMap &input, bool elevated)
{
    const QString path = expandPath(input.value(u"path"_s).toString());
    const QString content = input.value(u"content"_s).toString();

    if (elevated) {
        // Root writes go through a staged copy so the privileged half is one
        // predictable install(1) rather than a shell holding the file's text.
        auto *staged = new QTemporaryFile(this);
        if (!staged->open()) {
            staged->deleteLater();
            return u"Cannot stage the new contents of %1."_s.arg(path);
        }
        staged->write(content.toUtf8());
        staged->flush();
        const QString stagedPath = staged->fileName();
        connect(this, &AiToolbox::completed, staged, &QObject::deleteLater,
                Qt::SingleShotConnection);
        runShell(id,
                 u"install -m 644 -D %1 %2"_s.arg(shellQuote(stagedPath), shellQuote(path)),
                 true);
        return {};
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return u"Cannot write %1: %2"_s.arg(path, file.errorString());
    }
    file.write(content.toUtf8());
    if (!file.commit()) {
        return u"Cannot save %1."_s.arg(path);
    }
    return u"Wrote %1 bytes to %2."_s.arg(content.toUtf8().size()).arg(path);
}

QString AiToolbox::listDirectory(const QVariantMap &input) const
{
    const QString path = expandPath(input.value(u"path"_s).toString());
    QDir dir(path);
    if (!dir.exists()) {
        return u"%1 is not a directory."_s.arg(path);
    }
    const auto entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    QStringList lines;
    for (const QFileInfo &entry : entries) {
        lines.append(entry.isDir() ? entry.fileName() + u'/'
                                   : u"%1  (%2 bytes)"_s.arg(entry.fileName()).arg(entry.size()));
        if (lines.size() >= 400) {
            lines.append(u"… and more"_s);
            break;
        }
    }
    return lines.isEmpty() ? u"%1 is empty."_s.arg(path) : lines.join(u'\n');
}

QString AiToolbox::remember(const QVariantMap &input) const
{
    const QString fact = input.value(u"fact"_s).toString().trimmed();
    if (fact.isEmpty()) {
        return u"Nothing to remember."_s;
    }
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                             .filePath(u"atoll/assistant-memory.md"_s);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return u"Cannot write the memory file."_s;
    }
    file.write(u"- %1\n"_s.arg(fact).toUtf8());
    return u"Noted."_s;
}

QString AiToolbox::packageManagerInstall(const QStringList &packages)
{
    const PackageManager manager = detectPackageManager();
    if (manager.program.isEmpty()) {
        return {};
    }
    QStringList quoted;
    for (const QString &package : packages) {
        quoted.append(shellQuote(package));
    }
    return manager.program + u' ' + manager.install.join(u' ') + u' ' + quoted.join(u' ');
}

QString AiToolbox::packageManagerUpdate()
{
    const PackageManager manager = detectPackageManager();
    if (manager.program.isEmpty()) {
        return {};
    }
    QString command = manager.program + u' ' + manager.update.join(u' ');
    // apt splits refreshing from upgrading; everything else does both at once.
    if (manager.program == u"apt-get"_s) {
        command += u" && apt-get upgrade -y"_s;
    }
    return command;
}

void AiToolbox::execute(const AiToolCall &call, AiRisk risk)
{
    const bool elevated = risk == AiRisk::Admin;

    if (call.name == u"run_command"_s) {
        runShell(call.id, call.input.value(u"command"_s).toString(), elevated);
        return;
    }
    if (call.name == u"read_file"_s) {
        finish(call.id, readFile(call.input));
        return;
    }
    if (call.name == u"write_file"_s) {
        const QString outcome = writeFile(call.id, call.input, elevated);
        if (!outcome.isEmpty()) {
            finish(call.id, outcome, outcome.startsWith(u"Cannot"_s));
        }
        return;
    }
    if (call.name == u"list_directory"_s) {
        finish(call.id, listDirectory(call.input));
        return;
    }
    if (call.name == u"remember"_s) {
        finish(call.id, remember(call.input));
        return;
    }
    if (call.name == u"notify"_s) {
        const QString summary = call.input.value(u"summary"_s).toString();
        Q_EMIT messageRequested(summary, call.input.value(u"body"_s).toString());
        finish(call.id, u"Shown."_s);
        return;
    }
    if (call.name == u"take_screenshot"_s) {
        m_pending = call.id;
        m_capture->capture();
        return;
    }
    if (call.name == u"install_packages"_s) {
        const QString command = packageManagerInstall(call.input.value(u"packages"_s).toStringList());
        if (command.isEmpty()) {
            finish(call.id, u"No supported package manager was found on this machine."_s, true);
            return;
        }
        runShell(call.id, command, true);
        return;
    }
    if (call.name == u"system_update"_s) {
        const QString command = packageManagerUpdate();
        if (command.isEmpty()) {
            finish(call.id, u"No supported package manager was found on this machine."_s, true);
            return;
        }
        runShell(call.id, command, true);
        return;
    }
    if (call.name == u"set_wallpaper"_s) {
        const QString path = expandPath(call.input.value(u"path"_s).toString());
        if (!QFile::exists(path)) {
            finish(call.id, u"There is no file at %1."_s.arg(path), true);
            return;
        }
        const QString applier = QStandardPaths::findExecutable(u"plasma-apply-wallpaperimage"_s);
        if (!applier.isEmpty()) {
            runShell(call.id, u"plasma-apply-wallpaperimage %1"_s.arg(shellQuote(path)), false);
            return;
        }
        // Older sessions have no helper, but the shell will still evaluate a
        // script that walks the containments and swaps the image.
        const QString script =
            uR"(var all = desktops(); for (i = 0; i < all.length; i++) { d = all[i];
d.wallpaperPlugin = "org.kde.image";
d.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
d.writeConfig("Image", "file://%1"); })"_s.arg(path);
        runShell(call.id,
                 u"qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript %1"_s
                     .arg(shellQuote(script)),
                 false);
        return;
    }
    if (call.name == u"plasma_setting"_s) {
        const QString writer = QStandardPaths::findExecutable(u"kwriteconfig6"_s).isEmpty()
                                   ? u"kwriteconfig5"_s
                                   : u"kwriteconfig6"_s;
        QStringList groups = call.input.value(u"group"_s).toString().split(u'/', Qt::SkipEmptyParts);
        QString groupArguments;
        for (const QString &group : std::as_const(groups)) {
            groupArguments += u" --group "_s + shellQuote(group);
        }
        const QString command = u"%1 --file %2%3 --key %4 %5"_s
                                    .arg(writer,
                                         shellQuote(call.input.value(u"file"_s).toString()),
                                         groupArguments,
                                         shellQuote(call.input.value(u"key"_s).toString()),
                                         shellQuote(call.input.value(u"value"_s).toString()));
        runShell(call.id, command, false);
        return;
    }
    if (call.name == u"launch_app"_s) {
        const QString target = call.input.value(u"target"_s).toString();
        QString command;
        if (target.endsWith(u".desktop"_s) && !QStandardPaths::findExecutable(u"kstart"_s).isEmpty()) {
            command = u"kstart %1"_s.arg(shellQuote(target));
        } else if (target.contains(u"://"_s) || QFile::exists(expandPath(target))) {
            command = u"xdg-open %1"_s.arg(shellQuote(target.contains(u"://"_s) ? target : expandPath(target)));
        } else {
            command = u"setsid -f %1"_s.arg(target);
        }
        runShell(call.id, command, false);
        return;
    }

    finish(call.id, u"There is no tool called %1."_s.arg(call.name), true);
}

void AiToolbox::cancel()
{
    m_pending.clear();
    m_runner->cancelAll();
}
