/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "screencapture.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
/** What the providers will accept without re-encoding it themselves. */
constexpr int MaxEdge = 1568;

QString portalService()
{
    return u"org.freedesktop.portal.Desktop"_s;
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

void ScreenCapture::capture()
{
    m_portalTried = false;
    viaPortal();
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
        process->start(spectacle, {u"-b"_s, u"-n"_s, u"-f"_s, u"-o"_s, path});
        return;
    }

    const QString grim = QStandardPaths::findExecutable(u"grim"_s);
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

    if (image.width() > MaxEdge || image.height() > MaxEdge) {
        image = image.scaled(MaxEdge, MaxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    Q_EMIT captured(png);
}
