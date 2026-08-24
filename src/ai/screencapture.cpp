/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "screencapture.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
/** The name that means "do not single an output out". */
bool meansEverything(const QString &screenName)
{
    return screenName.isEmpty() || screenName.compare(u"all"_s, Qt::CaseInsensitive) == 0;
}

QString portalService()
{
    return u"org.freedesktop.portal.Desktop"_s;
}

QScreen *screenNamed(const QString &name)
{
    if (name.isEmpty()) {
        return QGuiApplication::primaryScreen();
    }
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name().compare(name, Qt::CaseInsensitive) == 0) {
            return screen;
        }
    }
    return nullptr;
}

/** The whole desktop, in the logical coordinates the outputs are laid out in. */
QRect desktopGeometry()
{
    QRect union_;
    const auto screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        union_ = union_.united(screen->geometry());
    }
    return union_;
}
}

ScreenCapture::ScreenCapture(QObject *parent)
    : QObject(parent)
{
}

bool ScreenCapture::available()
{
    auto bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && bus.interface() && bus.interface()->isServiceRegistered(portalService())) {
        return true;
    }
    return !QStandardPaths::findExecutable(u"spectacle"_s).isEmpty()
        || !QStandardPaths::findExecutable(u"grim"_s).isEmpty();
}

QVariantList ScreenCapture::outputs()
{
    QVariantList list;
    const QScreen *primary = QGuiApplication::primaryScreen();
    const auto screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        const QSize pixels = screen->geometry().size() * screen->devicePixelRatio();
        list.append(QVariantMap{
            {u"name"_s, screen->name()},
            {u"label"_s, labelFor(screen->name())},
            {u"width"_s, pixels.width()},
            {u"height"_s, pixels.height()},
            {u"primary"_s, screen == primary},
        });
    }
    return list;
}

bool ScreenCapture::hasSeveralOutputs()
{
    return QGuiApplication::screens().size() > 1;
}

QString ScreenCapture::currentOutputName()
{
    // Where the pointer is, because that is where the person is looking and
    // what they mean by "this screen".
    if (QScreen *under = QGuiApplication::screenAt(QCursor::pos())) {
        return under->name();
    }
    const QScreen *primary = QGuiApplication::primaryScreen();
    return primary ? primary->name() : QString();
}

QString ScreenCapture::labelFor(const QString &screenName)
{
    const QScreen *screen = screenNamed(screenName);
    if (!screen) {
        return screenName;
    }

    // The connector alone ("DP-3") tells somebody with three identical screens
    // nothing, so the model name comes with it where the monitor reports one.
    QString model = screen->model().trimmed();
    if (model.isEmpty() || model == screen->name()) {
        model = screen->manufacturer().trimmed();
    }
    const QSize pixels = screen->geometry().size() * screen->devicePixelRatio();
    QString label = screen->name();
    if (!model.isEmpty()) {
        label += u" - "_s + model;
    }
    label += u" (%1x%2)"_s.arg(pixels.width()).arg(pixels.height());
    if (screen == QGuiApplication::primaryScreen() && QGuiApplication::screens().size() > 1) {
        label += QCoreApplication::translate("ScreenCapture", ", main");
    }
    return label;
}

bool ScreenCapture::hasOutput(const QString &screenName)
{
    return !meansEverything(screenName) && screenNamed(screenName) != nullptr;
}

void ScreenCapture::setMaxEdge(int pixels)
{
    m_maxEdge = qBound(320, pixels, 4096);
}

void ScreenCapture::capture(const QString &screenName)
{
    m_screen = meansEverything(screenName) ? QString() : screenName;
    if (!m_screen.isEmpty() && !screenNamed(m_screen)) {
        Q_EMIT failed(tr("There is no screen called %1 on this machine.").arg(m_screen));
        return;
    }
    m_portalTried = false;
    m_grimTried = false;
    m_alreadyCut = false;
    viaPortal();
}

bool ScreenCapture::wantsOneScreen() const
{
    return !m_screen.isEmpty() && QGuiApplication::screens().size() > 1;
}

QRectF ScreenCapture::fractionOfDesktop() const
{
    const QScreen *screen = screenNamed(m_screen);
    const QRect desktop = desktopGeometry();
    if (!screen || desktop.width() <= 0 || desktop.height() <= 0) {
        return QRectF(0, 0, 1, 1);
    }
    const QRect wanted = screen->geometry();
    return QRectF(qreal(wanted.x() - desktop.x()) / desktop.width(),
                  qreal(wanted.y() - desktop.y()) / desktop.height(),
                  qreal(wanted.width()) / desktop.width(),
                  qreal(wanted.height()) / desktop.height());
}

void ScreenCapture::viaPortal()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()
        || !bus.interface()->isServiceRegistered(portalService())) {
        viaHelper();
        return;
    }

    m_portalTried = true;
    const QString handleToken = u"atoll_%1"_s.arg(++m_token);

    // The portal replies on a Request object whose path it derives from our
    // unique bus name and the token we hand it, so the subscription has to be
    // in place before the call is made.
    QString sender = bus.baseService();
    sender.remove(0, 1); // the leading colon
    sender.replace(u'.', u'_');
    const QString requestPath = u"/org/freedesktop/portal/desktop/request/%1/%2"_s.arg(sender, handleToken);

    bus.connect(portalService(),
                requestPath,
                u"org.freedesktop.portal.Request"_s,
                u"Response"_s,
                this,
                SLOT(handlePortalResponse(uint, QVariantMap)));

    QVariantMap options;
    options.insert(u"handle_token"_s, handleToken);
    // Non-interactive means "the whole screen, no editor": the portal still
    // asks the user for permission the first time, which is as it should be.
    // The portal has no notion of one output, so a single screen is cut out of
    // the picture afterwards rather than asked for here.
    options.insert(u"interactive"_s, false);
    options.insert(u"modal"_s, false);

    auto message = QDBusMessage::createMethodCall(portalService(),
                                                  u"/org/freedesktop/portal/desktop"_s,
                                                  u"org.freedesktop.portal.Screenshot"_s,
                                                  u"Screenshot"_s);
    message << QString() << options;

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 30000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        if (watcher->isError()) {
            viaHelper();
        }
    });

    // A portal that accepts the call and then never answers would leave the
    // assistant waiting forever, so the helpers get their turn regardless.
    QTimer::singleShot(35000, this, [this] {
        if (m_portalTried) {
            m_portalTried = false;
            viaHelper();
        }
    });
}

void ScreenCapture::handlePortalResponse(uint response, const QVariantMap &results)
{
    if (!m_portalTried) {
        return; // A helper already answered; this is a late reply.
    }
    m_portalTried = false;

    if (response != 0) {
        // 1 is the user saying no, and that is an answer, not a fault to route
        // around by shelling out to a screenshot tool behind their back.
        Q_EMIT failed(response == 1 ? tr("You declined the screenshot.")
                                    : tr("The desktop portal refused the screenshot."));
        return;
    }

    const QUrl uri(results.value(u"uri"_s).toString());
    const QString path = uri.isLocalFile() ? uri.toLocalFile() : results.value(u"uri"_s).toString();
    if (path.isEmpty()) {
        Q_EMIT failed(tr("The desktop portal returned no image."));
        return;
    }
    // The portal writes into a cache directory it owns and expects the file to
    // be taken away once it has been read.
    deliver(path, true);
}

void ScreenCapture::viaHelper()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = QDir(directory).filePath(u"atoll-screen-%1.png"_s.arg(QCoreApplication::applicationPid()));
    QFile::remove(path);

    // grim is the only one of these that can be pointed at an output by name,
    // so where it works it does the cutting itself and nothing is thrown away.
    // It only works under a wlroots compositor, and there is no way to know
    // that beforehand, so failure here is a fall-through rather than an error.
    const QString grim = QStandardPaths::findExecutable(u"grim"_s);
    if (!grim.isEmpty() && wantsOneScreen() && !m_grimTried) {
        m_grimTried = true;
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, path] {
            process->deleteLater();
            if (QFile::exists(path)) {
                m_alreadyCut = true;
                deliver(path, true);
                return;
            }
            viaHelper();
        });
        process->start(grim, {u"-o"_s, m_screen, path});
        return;
    }

    const QString spectacle = QStandardPaths::findExecutable(u"spectacle"_s);
    if (!spectacle.isEmpty()) {
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, path] {
            process->deleteLater();
            if (QFile::exists(path)) {
                deliver(path, true);
            } else {
                Q_EMIT failed(tr("Spectacle did not produce an image."));
            }
        });
        // -b runs it without its window, -n skips the "saved" notification.
        // -m is the monitor the pointer is on, which is the one output
        // Spectacle can name; anything else is cut out of the whole desktop.
        const bool pointerScreen = wantsOneScreen()
            && m_screen.compare(currentOutputName(), Qt::CaseInsensitive) == 0;
        m_alreadyCut = pointerScreen;
        process->start(spectacle,
                       {u"-b"_s, u"-n"_s, pointerScreen ? u"-m"_s : u"-f"_s, u"-o"_s, path});
        return;
    }

    if (!grim.isEmpty()) {
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, path] {
            process->deleteLater();
            if (QFile::exists(path)) {
                deliver(path, true);
            } else {
                Q_EMIT failed(tr("grim did not produce an image."));
            }
        });
        process->start(grim, {path});
        return;
    }

    Q_EMIT failed(tr("Nothing on this system can take a screenshot. Install xdg-desktop-portal-kde "
                     "or Spectacle."));
}

void ScreenCapture::deliver(const QString &path, bool temporary)
{
    QImage image(path);
    if (temporary) {
        QFile::remove(path);
    }
    if (image.isNull()) {
        Q_EMIT failed(tr("The screenshot could not be read back."));
        return;
    }

    // A picture that is already one output - grim's, or Spectacle's -m - must
    // not be cut again, or what comes back is a corner of it.
    if (wantsOneScreen() && !m_alreadyCut) {
        const QRectF fraction = fractionOfDesktop();
        const QRect cut = QRect(qRound(fraction.x() * image.width()),
                                qRound(fraction.y() * image.height()),
                                qRound(fraction.width() * image.width()),
                                qRound(fraction.height() * image.height()))
                              .intersected(image.rect());
        if (cut.width() > 16 && cut.height() > 16) {
            image = image.copy(cut);
        }
    }

    const int edge = qMax(m_maxEdge, 320);
    if (image.width() > edge || image.height() > edge) {
        image = image.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    Q_EMIT captured(png);
}
