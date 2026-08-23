/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Atoll - a dynamic island for KDE Plasma.
 */
#include "app/application.h"
#include "app/imagestore.h"

#include <LayerShellQt/Shell>

#include <KLocalizedString>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace
{
/** Forward a one-shot command to an already running instance. */
bool sendToRunningInstance(const QString &method)
{
    auto message = QDBusMessage::createMethodCall(u"org.atoll.Atoll"_s, u"/Atoll"_s, u"org.atoll.Atoll"_s, method);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    return reply.type() != QDBusMessage::ErrorMessage;
}
}

int main(int argc, char *argv[])
{
    QGuiApplication::setDesktopSettingsAware(true);
    // Layer-shell surfaces must be requested before the first window is created.
    // ATOLL_NO_LAYER_SHELL falls back to an ordinary top-level window, which is
    // how the island can be inspected under X11, in a nested compositor, or
    // when a compositor's layer-shell support is misbehaving.
    if (qEnvironmentVariableIntValue("ATOLL_NO_LAYER_SHELL") <= 0) {
        LayerShellQt::Shell::useLayerShell();
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(u"atoll"_s);
    QGuiApplication::setApplicationDisplayName(u"Atoll"_s);
    QGuiApplication::setApplicationVersion(QStringLiteral(ATOLL_VERSION));
    QGuiApplication::setOrganizationName(u"atoll"_s);
    QGuiApplication::setDesktopFileName(u"io.github.atoll.Atoll"_s);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("atoll"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("A dynamic island for KDE Plasma: a morphing overlay that mirrors\n"
                       "Plasma's OSD, your notifications and whatever is playing."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption toggleOption(u"toggle"_s, u"Expand or collapse the running island and exit."_s);
    const QCommandLineOption expandOption(u"expand"_s, u"Expand the running island and exit."_s);
    const QCommandLineOption collapseOption(u"collapse"_s, u"Collapse the running island and exit."_s);
    const QCommandLineOption quitOption(u"quit"_s, u"Ask the running island to exit."_s);
    parser.addOption(toggleOption);
    parser.addOption(expandOption);
    parser.addOption(collapseOption);
    parser.addOption(quitOption);
    parser.process(app);

    for (const auto &[option, method] : {std::pair{toggleOption, u"toggle"_s},
                                         std::pair{expandOption, u"expand"_s},
                                         std::pair{collapseOption, u"collapse"_s},
                                         std::pair{quitOption, u"quit"_s}}) {
        if (parser.isSet(option)) {
            if (sendToRunningInstance(method)) {
                return 0;
            }
            qWarning("atoll: no running instance to talk to");
            return 1;
        }
    }

    QQuickStyle::setStyle(u"Basic"_s);

    Application backend(nullptr);

    QQmlApplicationEngine engine;
    engine.addImageProvider(u"atoll"_s, new ImageStoreProvider);
    engine.addImageProvider(u"icon"_s, new IconProvider);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] {
            qCritical("atoll: the island failed to load");
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("Atoll", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    backend.start();

    // ATOLL_DEBUG_GRAB=<path> writes one rendered frame to disk and exits;
    // it separates "the island did not draw" from "the compositor did not
    // show it", which are otherwise indistinguishable from the outside.
    const QString grabPath = qEnvironmentVariable("ATOLL_DEBUG_GRAB");
    if (!grabPath.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const int delay = qEnvironmentVariableIntValue("ATOLL_DEBUG_GRAB_DELAY");
        QTimer::singleShot(delay > 0 ? delay : 3000, &app, [window, grabPath] {
            if (window) {
                const QImage frame = window->grabWindow();
                qWarning("atoll: grabbed %dx%d -> %s (saved: %d)",
                         frame.width(), frame.height(), qUtf8Printable(grabPath),
                         int(frame.save(grabPath)));
            }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
