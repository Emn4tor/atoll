/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "shellwindow.h"

#include "config/config.h"

#ifdef ATOLL_LOCKSCREEN_OVERLAY
#include "wayland/lockscreenoverlay.h"
#endif

#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>

using namespace Qt::StringLiterals;

namespace
{
LayerShellQt::Window::Layer layerFromName(const QString &name)
{
    if (name.compare(u"background"_s, Qt::CaseInsensitive) == 0) {
        return LayerShellQt::Window::LayerBackground;
    }
    if (name.compare(u"bottom"_s, Qt::CaseInsensitive) == 0) {
        return LayerShellQt::Window::LayerBottom;
    }
    if (name.compare(u"top"_s, Qt::CaseInsensitive) == 0) {
        return LayerShellQt::Window::LayerTop;
    }
    return LayerShellQt::Window::LayerOverlay;
}

bool isBottom(const QString &position)
{
    return position.startsWith(u"bottom"_s, Qt::CaseInsensitive);
}
}

ShellWindow::ShellWindow(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    watchScreens();
    recomputeTargets();

    connect(m_config, &Config::changed, this, [this] {
        recomputeTargets();
        reapply();
    });

    // The island is meant to live on the main monitor, so it has to follow the
    // main monitor: docking a laptop or unplugging a screen moves it.
    const auto onScreensChanged = [this] {
        watchScreens();
        Q_EMIT screensChanged();
        recomputeTargets();
        reapply();
    };
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, onScreensChanged);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, onScreensChanged);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, onScreensChanged);
}

bool ShellWindow::lockScreenSupported() const
{
#ifdef ATOLL_LOCKSCREEN_OVERLAY
    return LockscreenOverlay::instance().available();
#else
    return false;
#endif
}

QString ShellWindow::lockScreenReason() const
{
#ifdef ATOLL_LOCKSCREEN_OVERLAY
    return LockscreenOverlay::instance().unavailableReason();
#else
    return QStringLiteral("built without wayland-scanner");
#endif
}

bool ShellWindow::layerShellAvailable() const
{
    if (qEnvironmentVariableIntValue("ATOLL_NO_LAYER_SHELL") > 0) {
        return false;
    }
    return QGuiApplication::platformName().startsWith(u"wayland"_s);
}

void ShellWindow::watchScreens()
{
    for (QScreen *screen : QGuiApplication::screens()) {
        // Qt::UniqueConnection does not apply to lambdas, so the set is what
        // keeps a hotplug from connecting the same output twice.
        if (m_watched.contains(screen)) {
            continue;
        }
        m_watched.insert(screen);
        connect(screen, &QScreen::geometryChanged, this, [this] {
            Q_EMIT screensChanged();
            reapply();
        });
        connect(screen, &QObject::destroyed, this, [this](QObject *gone) {
            m_watched.remove(gone);
        });
    }
}

QScreen *ShellWindow::screenByName(const QString &name) const
{
    if (name.isEmpty() || name == u"primary"_s) {
        return QGuiApplication::primaryScreen();
    }
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name().compare(name, Qt::CaseInsensitive) == 0) {
            return screen;
        }
    }
    return nullptr;
}

QString ShellWindow::primaryScreen() const
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    return screen ? screen->name() : QString();
}

QVariantList ShellWindow::screens() const
{
    QVariantList list;
    const QScreen *primary = QGuiApplication::primaryScreen();
    for (const QScreen *screen : QGuiApplication::screens()) {
        list.append(QVariantMap{
            {u"name"_s, screen->name()},
            {u"manufacturer"_s, screen->manufacturer()},
            {u"model"_s, screen->model()},
            {u"primary"_s, screen == primary},
            {u"width"_s, screen->geometry().width()},
            {u"height"_s, screen->geometry().height()},
            {u"x"_s, screen->geometry().x()},
            {u"y"_s, screen->geometry().y()},
        });
    }
    return list;
}

void ShellWindow::recomputeTargets()
{
    QStringList wanted = m_config->value(u"island.screens"_s).toStringList();
    if (wanted.isEmpty()) {
        // Configs written before islands could be placed per output.
        wanted = QStringList{m_config->value(u"island.screen"_s, u"primary"_s).toString()};
    }

    QStringList resolved;
    const auto add = [&resolved](const QScreen *screen) {
        // Nested compositors and the offscreen platform hand out unnamed
        // outputs; "primary" still resolves to one of those.
        const QString name = screen && !screen->name().isEmpty() ? screen->name() : u"primary"_s;
        if (!resolved.contains(name)) {
            resolved.append(name);
        }
    };

    for (const QString &entry : std::as_const(wanted)) {
        if (entry.compare(u"all"_s, Qt::CaseInsensitive) == 0) {
            for (const QScreen *screen : QGuiApplication::screens()) {
                add(screen);
            }
            continue;
        }
        if (const QScreen *screen = screenByName(entry)) {
            add(screen);
        }
    }

    // Never end up with nothing: an island nobody can see is indistinguishable
    // from a crash, and the main monitor is the one place it always belongs.
    if (resolved.isEmpty()) {
        add(QGuiApplication::primaryScreen());
    }

    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: island targets = %s", qUtf8Printable(resolved.join(u", "_s)));
    }
    if (resolved != m_targets) {
        m_targets = resolved;
        Q_EMIT targetsChanged();
    }
}

QSize ShellWindow::surfaceSizeFor(QScreen *screen, int minimumHeight) const
{
    const int screenWidth = screen ? screen->geometry().width() : 1920;
    const int screenHeight = screen ? screen->geometry().height() : 1080;
    const int configured = m_config->value(u"island.surfaceHeight"_s, 700).toInt();
    const int height = qBound(240, qMax(configured, minimumHeight), screenHeight);
    return QSize(screenWidth, height);
}

int ShellWindow::surfaceWidthFor(const QString &screenName) const
{
    return surfaceSizeFor(screenByName(screenName)).width();
}

int ShellWindow::surfaceHeightFor(const QString &screenName) const
{
    return surfaceSizeFor(screenByName(screenName)).height();
}

void ShellWindow::ensureSurfaceHeight(QQuickWindow *window, int height)
{
    if (!window || height <= m_wanted.value(window)) {
        return;
    }
    // Round up so a spring animation does not commit a new surface size on
    // every frame it passes through.
    m_wanted.insert(window, ((height + 63) / 64) * 64);
    // The stage asks for room while the window is still being built, before
    // configure() has said which output it belongs to. Remembering the wish is
    // enough: configure() applies it along with everything else.
    if (m_windows.contains(window)) {
        reapplyOne(window, m_windows.value(window));
    }
}

void ShellWindow::configure(QQuickWindow *window, const QString &screenName)
{
    if (!window) {
        return;
    }
    m_windows.insert(window, screenName);
    connect(window, &QObject::destroyed, this, [this, window] {
        m_windows.remove(window);
        m_wanted.remove(window);
    });

    window->setFlag(Qt::FramelessWindowHint, true);
    // ATOLL_DEBUG_SURFACE paints the surface so its bounds are visible; in
    // that case leave the colour QML asked for alone.
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_SURFACE") <= 0) {
        window->setColor(Qt::transparent);
    }
    QScreen *screen = screenByName(screenName);
    if (screen) {
        window->setScreen(screen);
    }

    if (!layerShellAvailable()) {
        // X11 / nested fallback: an ordinary always-on-top frameless window.
        window->setFlag(Qt::WindowStaysOnTopHint, true);
        window->setFlag(Qt::Tool, true);
        reapplyOne(window, screenName);
        return;
    }

    auto *layer = LayerShellQt::Window::get(window);
    if (!layer) {
        return;
    }
    layer->setScope(u"atoll"_s);
    layer->setCloseOnDismissed(false);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    // The output a layer surface is placed on is picked from here, not from
    // QWindow::screen() - setting only the latter puts every island on
    // whichever output the compositor happens to prefer.
    layer->setScreen(screen);

    reapplyOne(window, screenName);

}

void ShellWindow::allowOnLockScreen(QQuickWindow *window)
{
#ifdef ATOLL_LOCKSCREEN_OVERLAY
    if (!window || !m_config->value(u"lockScreen.enabled"_s, true).toBool()) {
        return;
    }

    // The timing here is narrow on both sides. KWin resolves the request
    // against the window it has for the surface, so the surface must already
    // carry its layer-shell role - which Qt only gives it when the window is
    // shown - and the protocol requires the surface to still be unmapped,
    // which it is until the first frame is committed. Between showing the
    // window and rendering into it is the one moment that satisfies both.
    const bool asked = LockscreenOverlay::instance().allow(window);
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: lock screen overlay %s%s",
                 asked ? "requested" : "unavailable",
                 asked ? "" : qUtf8Printable(u" ("_s + LockscreenOverlay::instance().unavailableReason() + u")"_s));
    }
#else
    Q_UNUSED(window)
#endif
}

void ShellWindow::release(QQuickWindow *window)
{
    m_windows.remove(window);
    m_wanted.remove(window);
}

void ShellWindow::reapply()
{
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        reapplyOne(it.key(), it.value());
    }
    for (auto it = m_overlays.cbegin(); it != m_overlays.cend(); ++it) {
        reapplyOverlay(it.key(), it.value());
    }
}

void ShellWindow::configureOverlay(QQuickWindow *window, const QString &screenName)
{
    if (!window) {
        return;
    }
    m_overlays.insert(window, screenName);
    connect(window, &QObject::destroyed, this, [this, window] {
        m_overlays.remove(window);
    });

    window->setFlag(Qt::FramelessWindowHint, true);
    window->setColor(Qt::transparent);
    if (QScreen *screen = screenByName(screenName)) {
        window->setScreen(screen);
    }

    if (!layerShellAvailable()) {
        window->setFlag(Qt::WindowStaysOnTopHint, true);
        window->setFlag(Qt::Tool, true);
        window->setFlag(Qt::WindowTransparentForInput, true);
        reapplyOverlay(window, screenName);
        return;
    }

    if (auto *layer = LayerShellQt::Window::get(window)) {
        layer->setScope(u"atoll-glow"_s);
        layer->setCloseOnDismissed(false);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setScreen(screenByName(screenName));
    }
    reapplyOverlay(window, screenName);
}

void ShellWindow::reapplyOverlay(QQuickWindow *window, const QString &screenName)
{
    if (!window) {
        return;
    }
    QScreen *screen = screenByName(screenName);
    const QSize size = screen ? screen->geometry().size() : QSize(1920, 1080);

    if (layerShellAvailable()) {
        if (auto *layer = LayerShellQt::Window::get(window)) {
            if (screen) {
                layer->setScreen(screen);
            }
            // All four edges: the glow traces the outline of the output, so
            // the surface has to be the output.
            layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop
                                                            | LayerShellQt::Window::AnchorBottom
                                                            | LayerShellQt::Window::AnchorLeft
                                                            | LayerShellQt::Window::AnchorRight));
            layer->setLayer(LayerShellQt::Window::LayerOverlay);
            // -1 means "ignore every panel's reserved space"; the glow is
            // decoration and must not push anything around.
            layer->setExclusiveZone(-1);
            layer->setMargins(QMargins());
            layer->setDesiredSize(size);
        }
    } else if (QScreen *host = window->screen()) {
        window->setGeometry(host->geometry());
    }

    window->resize(size);
    // Nothing here is ever clickable: an empty mask makes the whole surface
    // transparent to the pointer, which is exactly what is wanted.
    window->setMask(QRegion());
}

void ShellWindow::reapplyOne(QQuickWindow *window, const QString &screenName)
{
    if (!window) {
        return;
    }

    // The screen is deliberately not re-assigned here. A mapped layer surface
    // keeps the output it was created on, and pushing QWindow::screen() back
    // to our choice only makes Qt tear the surface down and build it again,
    // for as long as the compositor disagrees. Moving to another output means
    // a new window, which is what the target list already produces.
    QScreen *screen = screenByName(screenName);

    const QSize size = surfaceSizeFor(screen, m_wanted.value(window));
    const QString position = m_config->value(u"island.position"_s, u"top-center"_s).toString();
    const bool bottom = isBottom(position);
    // A notch is defined by touching the screen edge, so it does not get to
    // float on a margin; only a pill does.
    const bool notched = m_config->value(u"island.shape"_s, u"notch"_s).toString() != u"pill"_s;
    const int edgeMargin = notched ? 0
                                   : m_config->value(u"island.edgeMargin"_s,
                                                     m_config->value(u"island.topMargin"_s, 0))
                                         .toInt();
    // Overlapping the panels is the whole point of an island that sits on top
    // of the shell; -1 tells the compositor to ignore every exclusive zone.
    const bool overlap = m_config->value(u"island.overlapPanels"_s, true).toBool();
    const int exclusiveZone = overlap ? -1 : m_config->value(u"island.exclusiveZone"_s, 0).toInt();

    if (layerShellAvailable()) {
        if (auto *layer = LayerShellQt::Window::get(window)) {
            if (screen) {
                layer->setScreen(screen);
            }
            // Anchoring to one edge only lets the compositor centre us on the
            // other axis; the island itself is then placed inside the surface.
            layer->setAnchors(LayerShellQt::Window::Anchors(bottom ? LayerShellQt::Window::AnchorBottom
                                                                   : LayerShellQt::Window::AnchorTop));
            layer->setLayer(layerFromName(m_config->value(u"island.layer"_s, u"overlay"_s).toString()));
            layer->setExclusiveZone(exclusiveZone);
            layer->setMargins(QMargins(0, bottom ? 0 : edgeMargin, 0, bottom ? edgeMargin : 0));
            layer->setDesiredSize(size);
        }
    } else if (QScreen *host = window->screen()) {
        const QRect geo = host->geometry();
        window->setX(geo.x() + (geo.width() - size.width()) / 2);
        window->setY(bottom ? geo.y() + geo.height() - size.height() - edgeMargin : geo.y() + edgeMargin);
    }

    window->resize(size);

    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: surface on %s -> screen=%s layerShell=%d anchor=%s zone=%d margin=%d size=%dx%d",
                 qUtf8Printable(screenName),
                 qUtf8Printable(screen ? screen->name() : u"<none>"_s),
                 int(layerShellAvailable() && LayerShellQt::Window::get(window) != nullptr),
                 bottom ? "bottom" : "top",
                 exclusiveZone,
                 edgeMargin,
                 size.width(),
                 size.height());
    }
}

void ShellWindow::setInputRegion(QQuickWindow *window, const QVariantList &rects)
{
    if (!window) {
        return;
    }

    QRegion region;
    for (const QVariant &entry : rects) {
        const QRectF r = entry.toRectF();
        if (r.width() <= 0 || r.height() <= 0) {
            continue;
        }
        region += r.toAlignedRect();
    }

    // An empty mask would make the surface entirely click-through, including
    // the island itself, so fall back to nothing rather than to everything.
    window->setMask(region);
}

void ShellWindow::setKeyboardFocus(QQuickWindow *window, bool wanted)
{
    if (!window || !layerShellAvailable()) {
        return;
    }
    if (auto *layer = LayerShellQt::Window::get(window)) {
        layer->setKeyboardInteractivity(wanted ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                               : LayerShellQt::Window::KeyboardInteractivityNone);
    }
}
