/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "shellwindow.h"

#include "config/config.h"

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
}

ShellWindow::ShellWindow(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    recomputeSurfaceSize();
    connect(m_config, &Config::changed, this, [this] {
        recomputeSurfaceSize();
        reapply();
    });
}

bool ShellWindow::layerShellAvailable() const
{
    if (qEnvironmentVariableIntValue("ATOLL_NO_LAYER_SHELL") > 0) {
        return false;
    }
    return QGuiApplication::platformName().startsWith(u"wayland"_s);
}

QScreen *ShellWindow::targetScreen() const
{
    const QString wanted = m_config->value(u"island.screen"_s, u"primary"_s).toString();
    if (wanted.isEmpty() || wanted == u"primary"_s) {
        return QGuiApplication::primaryScreen();
    }
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name().compare(wanted, Qt::CaseInsensitive) == 0) {
            return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}

void ShellWindow::recomputeSurfaceSize()
{
    const QScreen *screen = targetScreen();
    const int screenWidth = screen ? screen->geometry().width() : 1920;
    const int screenHeight = screen ? screen->geometry().height() : 1080;

    const int height = qBound(240, m_config->value(u"island.surfaceHeight"_s, 520).toInt(), screenHeight);
    const QSize size(screenWidth, height);

    if (size != m_surfaceSize) {
        m_surfaceSize = size;
        Q_EMIT surfaceSizeChanged();
    }
}

void ShellWindow::configure(QQuickWindow *window)
{
    if (!window) {
        return;
    }
    m_window = window;

    window->setFlag(Qt::FramelessWindowHint, true);
    // ATOLL_DEBUG_SURFACE paints the surface so its bounds are visible; in
    // that case leave the colour QML asked for alone.
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_SURFACE") <= 0) {
        window->setColor(Qt::transparent);
    }
    if (QScreen *screen = targetScreen()) {
        window->setScreen(screen);
    }

    if (!layerShellAvailable()) {
        // X11 / nested fallback: an ordinary always-on-top frameless window.
        window->setFlag(Qt::WindowStaysOnTopHint, true);
        window->setFlag(Qt::Tool, true);
        reapply();
        return;
    }

    auto *layer = LayerShellQt::Window::get(window);
    if (!layer) {
        return;
    }
    layer->setScope(u"atoll"_s);
    layer->setCloseOnDismissed(false);
    // Anchoring only to the top lets the compositor centre us horizontally,
    // which is exactly the notch position we want.
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop));
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

    reapply();
}

void ShellWindow::reapply()
{
    if (!m_window) {
        return;
    }

    if (QScreen *screen = targetScreen(); screen && m_window->screen() != screen) {
        m_window->setScreen(screen);
    }

    if (layerShellAvailable()) {
        if (auto *layer = LayerShellQt::Window::get(m_window)) {
            layer->setLayer(layerFromName(m_config->value(u"island.layer"_s, u"overlay"_s).toString()));
            layer->setExclusiveZone(m_config->value(u"island.exclusiveZone"_s, 0).toInt());
            layer->setMargins(QMargins(0, m_config->value(u"island.topMargin"_s, 0).toInt(), 0, 0));
            layer->setDesiredSize(m_surfaceSize);
        }
    } else if (QScreen *screen = m_window->screen()) {
        const QRect geo = screen->geometry();
        m_window->setX(geo.x() + (geo.width() - m_surfaceSize.width()) / 2);
        m_window->setY(geo.y() + m_config->value(u"island.topMargin"_s, 0).toInt());
    }

    m_window->resize(m_surfaceSize);
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
