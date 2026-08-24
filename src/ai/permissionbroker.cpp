/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "permissionbroker.h"

#include "config/config.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace
{
/**
 * Commands that only look at the machine. Everything here is something the
 * user could read in a file manager or a terminal without being asked for a
 * password, which is what makes it safe to run without interrupting them.
 */
const QStringList &readOnlyCommands()
{
    static const QStringList list = {
        u"awk"_s,       u"basename"_s,  u"cat"_s,      u"cksum"_s,    u"column"_s,   u"comm"_s,
        u"cut"_s,       u"date"_s,      u"df"_s,       u"diff"_s,     u"dirname"_s,  u"du"_s,
        u"echo"_s,      u"env"_s,       u"fastfetch"_s, u"file"_s,    u"find"_s,     u"free"_s,
        u"getent"_s,    u"grep"_s,      u"groups"_s,   u"head"_s,     u"hostname"_s, u"id"_s,
        u"inxi"_s,      u"jq"_s,        u"less"_s,     u"locale"_s,   u"lsattr"_s,   u"lsblk"_s,
        u"lscpu"_s,     u"lshw"_s,      u"lsmod"_s,    u"lsof"_s,     u"lspci"_s,    u"lsusb"_s,
        u"md5sum"_s,    u"nproc"_s,     u"od"_s,       u"pgrep"_s,    u"printenv"_s, u"printf"_s,
        u"ps"_s,        u"pwd"_s,       u"readlink"_s, u"realpath"_s, u"rg"_s,       u"sensors"_s,
        u"seq"_s,       u"sha256sum"_s, u"sort"_s,     u"stat"_s,     u"strings"_s,  u"tail"_s,
        u"tr"_s,        u"type"_s,      u"uname"_s,    u"uniq"_s,     u"uptime"_s,   u"vmstat"_s,
        u"wc"_s,        u"which"_s,     u"whoami"_s,   u"xdg-user-dir"_s, u"yes"_s,
    };
    return list;
}

/**
 * Tools that are read-only only in some of their moods. The second field is
 * the set of first arguments that keep them harmless; anything else falls
 * through to the ordinary rules.
 */
struct ConditionalRead {
    const char *program;
    QStringList safeVerbs;
};

const QList<ConditionalRead> &conditionalReads()
{
    static const QList<ConditionalRead> list = {
        {"systemctl", {u"status"_s, u"list-units"_s, u"list-unit-files"_s, u"list-timers"_s,
                       u"is-active"_s, u"is-enabled"_s, u"is-failed"_s, u"show"_s, u"cat"_s}},
        {"journalctl", {}}, // read-only by nature; writing needs a different tool
        {"pacman", {u"-Q"_s, u"-Qi"_s, u"-Qs"_s, u"-Ql"_s, u"-Qo"_s, u"-Qe"_s, u"-Qm"_s, u"-Qdt"_s,
                    u"-Si"_s, u"-Ss"_s, u"-Sl"_s, u"-Sg"_s, u"-F"_s, u"-Fy"_s}},
        {"paru", {u"-Q"_s, u"-Qi"_s, u"-Qs"_s, u"-Si"_s, u"-Ss"_s}},
        {"yay", {u"-Q"_s, u"-Qi"_s, u"-Qs"_s, u"-Si"_s, u"-Ss"_s}},
        {"flatpak", {u"list"_s, u"info"_s, u"search"_s, u"remotes"_s, u"history"_s}},
        {"git", {u"status"_s, u"log"_s, u"diff"_s, u"show"_s, u"branch"_s, u"remote"_s, u"config"_s}},
        {"wpctl", {u"status"_s, u"get-volume"_s, u"inspect"_s}},
        {"pactl", {u"list"_s, u"info"_s, u"stat"_s, u"get-sink-volume"_s, u"get-default-sink"_s}},
        {"nmcli", {u"device"_s, u"connection"_s, u"general"_s, u"networking"_s, u"radio"_s,
                   u"-t"_s, u"-f"_s, u"--terse"_s}},
        {"bluetoothctl", {u"show"_s, u"devices"_s, u"info"_s, u"paired-devices"_s}},
        {"timedatectl", {u"status"_s, u"show"_s, u"list-timezones"_s}},
        {"hostnamectl", {u"status"_s, u"show"_s}},
        {"localectl", {u"status"_s, u"list-locales"_s, u"list-keymaps"_s}},
        {"kreadconfig6", {}},
        {"kreadconfig5", {}},
        {"ip", {u"a"_s, u"addr"_s, u"r"_s, u"route"_s, u"link"_s, u"neigh"_s, u"-c"_s, u"-br"_s}},
        {"upower", {u"-i"_s, u"-d"_s, u"-e"_s, u"--dump"_s}},
        {"loginctl", {u"list-sessions"_s, u"show-session"_s, u"session-status"_s, u"user-status"_s}},
    };
    return list;
}

/** Programs that always mean "as root", whether or not sudo is spelled out. */
bool isAdminProgram(const QString &program)
{
    static const QStringList list = {
        u"blkid"_s,     u"bootctl"_s,   u"chpasswd"_s,  u"chroot"_s,    u"cryptsetup"_s,
        u"dracut"_s,    u"fdisk"_s,     u"firewall-cmd"_s, u"fsck"_s,   u"gparted"_s,
        u"groupadd"_s,  u"groupdel"_s,  u"grub-install"_s, u"grub-mkconfig"_s,
        u"insmod"_s,    u"iptables"_s,  u"mkinitcpio"_s, u"modprobe"_s, u"mount"_s,
        u"nft"_s,       u"parted"_s,    u"partprobe"_s, u"pwconv"_s,    u"rmmod"_s,
        u"sfdisk"_s,    u"swapoff"_s,   u"swapon"_s,    u"ufw"_s,       u"umount"_s,
        u"update-grub"_s, u"useradd"_s, u"usermod"_s,   u"visudo"_s,    u"vgchange"_s,
    };
    return list.contains(program);
}

/** The programs that a user's own session is never allowed to hand to a model. */
bool isForbiddenProgram(const QString &program)
{
    static const QStringList list = {
        u"mkfs"_s,     u"mkswap"_s,   u"shred"_s,    u"userdel"_s,  u"passwd"_s,
        u"gpasswd"_s,  u"wipefs"_s,   u"badblocks"_s, u"nc"_s,      u"ncat"_s,
        u"netcat"_s,   u"telnet"_s,
    };
    if (list.contains(program)) {
        return true;
    }
    return program.startsWith(u"mkfs."_s);
}

/** Directories that hold secrets. Nothing in here is readable or writable. */
bool isSecretPath(const QString &absolute)
{
    const QString home = QDir::homePath();

    // Boundary-aware throughout: a bare prefix test would make /etc/sudoers.d
    // match /etc/sudoers.d.backup, and /root match every path under a home
    // directory that happens to live there.
    const auto under = [&absolute](const QString &prefix) {
        return absolute == prefix || absolute.startsWith(prefix + u'/');
    };

    static const QStringList relative = {
        u"/.ssh"_s,
        u"/.gnupg"_s,
        u"/.password-store"_s,
        u"/.local/share/kwalletd"_s,
        u"/.local/share/keyrings"_s,
        u"/.config/atoll/credentials.json"_s,
        u"/.local/share/atoll/credentials.json"_s,
        u"/.pki"_s,
        u"/.aws"_s,
        u"/.netrc"_s,
    };
    for (const QString &tail : relative) {
        if (under(home + tail)) {
            return true;
        }
    }

    // Everything below is about files that belong to the system or to other
    // people. Inside the user's own home none of it applies - which matters
    // for the accounts whose home really is somewhere like /root.
    if (absolute == home || absolute.startsWith(home + u'/')) {
        return false;
    }

    static const QStringList system = {
        u"/etc/shadow"_s, u"/etc/gshadow"_s, u"/etc/sudoers"_s, u"/etc/sudoers.d"_s,
        u"/root"_s, u"/proc/kcore"_s,
    };
    for (const QString &prefix : system) {
        if (under(prefix)) {
            return true;
        }
    }
    // The host keys are a family of files rather than a directory, so this one
    // really is a plain prefix.
    return absolute.startsWith(u"/etc/ssh/ssh_host"_s);
}

/** Strip the shell noise that hides the real program name. */
QString programOf(const QString &segment)
{
    const QStringList words = QProcess::splitCommand(segment);
    for (const QString &word : words) {
        // Leading VAR=value assignments, and the wrappers that change nothing
        // about what is ultimately run.
        if (word.contains(u'=') && !word.startsWith(u'-') && !word.contains(u'/')) {
            continue;
        }
        if (word == u"command"_s || word == u"exec"_s || word == u"nohup"_s
            || word == u"nice"_s || word == u"time"_s || word == u"stdbuf"_s) {
            continue;
        }
        return QFileInfo(word).fileName();
    }
    return {};
}

/** The arguments after the program, wrappers already skipped. */
QStringList argumentsOf(const QString &segment)
{
    QStringList words = QProcess::splitCommand(segment);
    while (!words.isEmpty()) {
        const QString word = words.first();
        if ((word.contains(u'=') && !word.startsWith(u'-') && !word.contains(u'/'))
            || word == u"command"_s || word == u"exec"_s || word == u"nohup"_s
            || word == u"nice"_s || word == u"time"_s || word == u"stdbuf"_s) {
            words.removeFirst();
            continue;
        }
        words.removeFirst(); // the program itself
        break;
    }
    return words;
}

/** Split on the operators that chain one command into the next. */
QStringList segmentsOf(const QString &command)
{
    QStringList out;
    QString current;
    QChar quote;
    for (int i = 0; i < command.size(); ++i) {
        const QChar c = command.at(i);
        if (!quote.isNull()) {
            if (c == quote) {
                quote = QChar();
            }
            current.append(c);
            continue;
        }
        if (c == u'\'' || c == u'"') {
            quote = c;
            current.append(c);
            continue;
        }
        if (c == u';' || c == u'|' || c == u'&' || c == u'\n') {
            out.append(current);
            current.clear();
            continue;
        }
        // Command substitution counts as its own command, so unwrap it rather
        // than letting `echo $(rm -rf ~)` pass as an echo.
        if (c == u'$' && i + 1 < command.size() && command.at(i + 1) == u'(') {
            out.append(current);
            current.clear();
            i += 1;
            continue;
        }
        if (c == u'`' || c == u'(' || c == u')') {
            out.append(current);
            current.clear();
            continue;
        }
        current.append(c);
    }
    out.append(current);

    QStringList trimmed;
    for (const QString &segment : std::as_const(out)) {
        const QString value = segment.trimmed();
        if (!value.isEmpty()) {
            trimmed.append(value);
        }
    }
    return trimmed;
}
}

PermissionBroker::PermissionBroker(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

QString PermissionBroker::tierTitle(AiRisk risk)
{
    switch (risk) {
    case AiRisk::Safe:
        return tr("Only reads something");
    case AiRisk::User:
        return tr("Changes your files or your settings");
    case AiRisk::Admin:
        return tr("Needs administrator rights");
    case AiRisk::Forbidden:
        return tr("Not allowed");
    }
    return {};
}

bool PermissionBroker::tierEnabled(AiRisk risk) const
{
    const QString mode = m_config->value(u"ai.permissions.mode"_s, u"guarded"_s).toString();
    switch (risk) {
    case AiRisk::Safe:
        return true;
    case AiRisk::User:
        return mode != u"readonly"_s;
    case AiRisk::Admin:
        return mode != u"readonly"_s && m_config->value(u"ai.permissions.allowRoot"_s, true).toBool();
    case AiRisk::Forbidden:
        return false;
    }
    return false;
}

bool PermissionBroker::isPreApproved(const AiVerdict &verdict) const
{
    if (verdict.risk == AiRisk::Forbidden || !tierEnabled(verdict.risk)) {
        return false;
    }
    if (verdict.risk == AiRisk::Safe) {
        return true;
    }
    // Root is never remembered. A polkit prompt is the point, not an obstacle.
    if (verdict.risk == AiRisk::Admin) {
        return false;
    }
    if (m_config->value(u"ai.permissions.mode"_s, u"guarded"_s).toString() == u"trusted"_s) {
        return true;
    }
    return !verdict.grantKey.isEmpty() && m_sessionGrants.contains(verdict.grantKey);
}

void PermissionBroker::grantForSession(const QString &grantKey)
{
    if (!grantKey.isEmpty()) {
        m_sessionGrants.insert(grantKey);
    }
}

void PermissionBroker::revokeAll()
{
    m_sessionGrants.clear();
}

AiVerdict PermissionBroker::classifyPath(const QString &path, bool forWriting) const
{
    AiVerdict verdict;
    const QString absolute = QFileInfo(QDir::cleanPath(path.startsWith(u'~')
                                                           ? QDir::homePath() + path.mid(1)
                                                           : path))
                                 .absoluteFilePath();
    verdict.detail = absolute;

    if (isSecretPath(absolute)) {
        verdict.risk = AiRisk::Forbidden;
        verdict.refusal = tr("%1 holds credentials. Atoll never opens it for the assistant.")
                              .arg(absolute);
        return verdict;
    }

    const QString home = QDir::homePath();
    const bool inHome = absolute == home || absolute.startsWith(home + u'/');

    if (!forWriting) {
        // Reading anything the user could open themselves is not an event.
        verdict.risk = AiRisk::Safe;
        verdict.summary = tr("Read %1").arg(QFileInfo(absolute).fileName());
        verdict.grantKey = u"read"_s;
        return verdict;
    }

    verdict.risk = inHome ? AiRisk::User : AiRisk::Admin;
    // The name in the headline, the whole path in the detail below it: the
    // headline has to survive being one line on a pill.
    verdict.summary = tr("Write %1").arg(QFileInfo(absolute).fileName());
    verdict.grantKey = inHome ? u"write:home"_s : u"write:system"_s;
    return verdict;
}

AiVerdict PermissionBroker::classifyCommand(const QString &command) const
{
    AiVerdict verdict;
    verdict.detail = command;
    verdict.risk = AiRisk::Safe;

    const QString flat = command.simplified();

    // A handful of shapes are refused before anything is parsed, because their
    // damage is not proportional to how convincing the explanation was.
    static const QList<QRegularExpression> forbidden = {
        QRegularExpression(u"\\brm\\s+(-[a-zA-Z]*\\s+)*(-[a-zA-Z]*[rR][a-zA-Z]*)\\s+(-[a-zA-Z]+\\s+)*/\\s*$"_s),
        QRegularExpression(u"\\brm\\b[^|;]*\\s--no-preserve-root\\b"_s),
        QRegularExpression(u"\\bdd\\b[^|;]*\\bof=/dev/(sd|nvme|vd|hd|mmcblk)"_s),
        QRegularExpression(u">\\s*/dev/(sd|nvme|vd|hd|mmcblk)"_s),
        QRegularExpression(u":\\(\\)\\s*\\{.*\\|.*&.*\\}"_s),
        QRegularExpression(u"\\bchmod\\b\\s+(-[a-zA-Z]+\\s+)*777\\s+/\\s*$"_s),
        // Fetching a script and running it unread is how a machine gets owned,
        // and it hides everything the rules below would otherwise have caught.
        QRegularExpression(u"\\b(curl|wget)\\b[^|]*\\|\\s*(sudo\\s+)?(ba|z|k|da)?sh\\b"_s),
    };
    for (const QRegularExpression &pattern : forbidden) {
        if (pattern.match(flat).hasMatch()) {
            verdict.risk = AiRisk::Forbidden;
            verdict.refusal = tr("That command destroys data or runs unread code. Atoll will not run it.");
            return verdict;
        }
    }

    const QStringList segments = segmentsOf(command);
    QStringList programs;

    for (const QString &segment : segments) {
        const QString program = programOf(segment);
        if (program.isEmpty()) {
            continue;
        }
        programs.append(program);
        const QStringList args = argumentsOf(segment);

        if (isForbiddenProgram(program)) {
            verdict.risk = AiRisk::Forbidden;
            verdict.refusal = tr("`%1` can destroy the system or its accounts.").arg(program);
            return verdict;
        }

        // Explicit escalation, or a program that only exists as root.
        if (program == u"sudo"_s || program == u"pkexec"_s || program == u"doas"_s
            || program == u"su"_s || program == u"run0"_s || isAdminProgram(program)) {
            verdict.risk = AiRisk::Admin;
            continue;
        }

        if (readOnlyCommands().contains(program)) {
            continue;
        }
        // `sed -i` and `awk` writing through a redirect are edits, not reads.
        if (program == u"sed"_s && !args.contains(u"-i"_s)
            && std::none_of(args.cbegin(), args.cend(), [](const QString &a) {
                   return a.startsWith(u"-i"_s);
               })) {
            continue;
        }

        bool conditionalMatched = false;
        for (const ConditionalRead &entry : conditionalReads()) {
            if (program != QString::fromLatin1(entry.program)) {
                continue;
            }
            conditionalMatched = true;
            if (entry.safeVerbs.isEmpty()) {
                break; // read-only whatever the arguments
            }
            const QString verb = args.isEmpty() ? QString() : args.first();
            if (!entry.safeVerbs.contains(verb)) {
                // systemctl --user restart is the user's own session; the same
                // verb without --user reaches into the machine.
                verdict.risk = maxRisk(verdict.risk,
                                    args.contains(u"--user"_s) ? AiRisk::User : AiRisk::Admin);
            }
            break;
        }
        if (conditionalMatched) {
            continue;
        }

        // A redirect writes somewhere, and where decides the tier.
        if (segment.contains(u'>')) {
            verdict.risk = maxRisk(verdict.risk, AiRisk::User);
        }

        // Anything unrecognised is assumed to change the session.
        verdict.risk = maxRisk(verdict.risk, AiRisk::User);
    }

    if (programs.isEmpty()) {
        verdict.risk = AiRisk::User;
    }

    verdict.summary = flat.length() > 90 ? flat.left(87) + u"…"_s : flat;
    switch (verdict.risk) {
    case AiRisk::Admin:
        verdict.grantKey = u"command:admin"_s;
        break;
    case AiRisk::User:
        verdict.grantKey = u"command:"_s + (programs.isEmpty() ? u"shell"_s : programs.first());
        break;
    default:
        verdict.grantKey = u"command:read"_s;
        break;
    }
    return verdict;
}

AiVerdict PermissionBroker::classify(const AiToolCall &call) const
{
    AiVerdict verdict;

    if (call.name == u"run_command"_s) {
        verdict = classifyCommand(call.input.value(u"command"_s).toString());
        const QString purpose = call.input.value(u"purpose"_s).toString();
        if (!purpose.isEmpty() && verdict.risk != AiRisk::Forbidden) {
            verdict.summary = purpose;
        }
        return verdict;
    }

    if (call.name == u"read_file"_s || call.name == u"list_directory"_s) {
        verdict = classifyPath(call.input.value(u"path"_s).toString(), false);
        if (call.name == u"list_directory"_s && verdict.risk != AiRisk::Forbidden) {
            verdict.summary = tr("List %1").arg(verdict.detail);
        }
        return verdict;
    }

    if (call.name == u"write_file"_s) {
        verdict = classifyPath(call.input.value(u"path"_s).toString(), true);
        return verdict;
    }

    if (call.name == u"install_packages"_s) {
        const QStringList packages = call.input.value(u"packages"_s).toStringList();
        verdict.risk = AiRisk::Admin;
        verdict.summary = tr("Install %1").arg(packages.join(u", "_s));
        verdict.detail = tr("Package manager, as root: %1").arg(packages.join(u' '));
        verdict.grantKey = u"packages"_s;
        if (packages.isEmpty()) {
            verdict.risk = AiRisk::Forbidden;
            verdict.refusal = tr("No package was named.");
        }
        return verdict;
    }

    if (call.name == u"system_update"_s) {
        verdict.risk = AiRisk::Admin;
        verdict.summary = tr("Update the whole system");
        verdict.detail = tr("Refresh the package databases and upgrade every installed package.");
        verdict.grantKey = u"update"_s;
        return verdict;
    }

    if (call.name == u"set_wallpaper"_s) {
        verdict.risk = AiRisk::User;
        verdict.summary = tr("Change the wallpaper");
        verdict.detail = call.input.value(u"path"_s).toString();
        verdict.grantKey = u"wallpaper"_s;
        return verdict;
    }

    if (call.name == u"plasma_setting"_s) {
        verdict.risk = AiRisk::User;
        verdict.summary = tr("Change a Plasma setting");
        verdict.detail = u"%1 → [%2] %3 = %4"_s.arg(call.input.value(u"file"_s).toString(),
                                                    call.input.value(u"group"_s).toString(),
                                                    call.input.value(u"key"_s).toString(),
                                                    call.input.value(u"value"_s).toString());
        verdict.grantKey = u"plasma"_s;
        return verdict;
    }

    if (call.name == u"launch_app"_s) {
        verdict.risk = AiRisk::User;
        verdict.summary = tr("Open %1").arg(call.input.value(u"target"_s).toString());
        verdict.detail = call.input.value(u"target"_s).toString();
        verdict.grantKey = u"launch"_s;
        return verdict;
    }

    if (call.name == u"take_screenshot"_s) {
        verdict.risk = AiRisk::User;
        verdict.summary = tr("Look at your screen");
        verdict.detail = tr("One still picture of the current screen is sent to the assistant.");
        verdict.grantKey = u"screenshot"_s;
        return verdict;
    }

    // The client's own web tools. Both are the service reading a public page
    // on the user's behalf - nothing on this machine changes, and nothing the
    // user could not have opened in a browser is reached - so neither is worth
    // stopping a person for. The address still shows in the detail line.
    if (call.name == u"web_search"_s) {
        verdict.risk = AiRisk::Safe;
        verdict.summary = tr("Search the web for %1").arg(call.input.value(u"query"_s).toString());
        verdict.detail = call.input.value(u"query"_s).toString();
        verdict.grantKey = u"web"_s;
        return verdict;
    }

    if (call.name == u"fetch_url"_s) {
        verdict.risk = AiRisk::Safe;
        verdict.summary = tr("Read a page from the web");
        verdict.detail = call.input.value(u"url"_s).toString();
        verdict.grantKey = u"web"_s;
        return verdict;
    }

    if (call.name == u"notify"_s || call.name == u"remember"_s) {
        verdict.risk = AiRisk::Safe;
        verdict.summary = call.name == u"notify"_s ? tr("Show a message") : tr("Remember a preference");
        verdict.grantKey = u"chatter"_s;
        return verdict;
    }

    verdict.risk = AiRisk::User;
    verdict.summary = call.name;
    verdict.detail = call.name;
    verdict.grantKey = u"tool:"_s + call.name;
    return verdict;
}
