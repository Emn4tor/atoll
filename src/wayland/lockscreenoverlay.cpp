/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lockscreenoverlay.h"

#include "kde-lockscreen-overlay-v1-client-protocol.h"

#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QWindow>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView InterfaceName{"kde_lockscreen_overlay_v1"};

wl_display *waylandDisplay()
{
    if (auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
        return native->display();
    }
    return nullptr;
}

wl_surface *waylandSurface(QWindow *window)
{
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    if (!native || !window) {
        return nullptr;
    }
    return static_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), window));
}

/**
 * Registry walk on a queue of our own.
 *
 * Qt drives the default queue from its event loop; dispatching that queue from
 * here to wait for a roundtrip would run Qt's own protocol handlers at a point
 * Qt does not expect. A private queue keeps the two apart.
 */
struct RegistryHunt {
    const char *wanted = nullptr;
    uint32_t name = 0;
    uint32_t version = 0;
    bool found = false;
};

void onGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    Q_UNUSED(registry)
    auto *hunt = static_cast<RegistryHunt *>(data);
    if (hunt->found || qstrcmp(interface, hunt->wanted) != 0) {
        return;
    }
    hunt->name = name;
    hunt->version = version;
    hunt->found = true;
}

void onGlobalRemove(void *data, wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
}

constexpr wl_registry_listener RegistryListener = {onGlobal, onGlobalRemove};
}

LockscreenOverlay &LockscreenOverlay::instance()
{
    static LockscreenOverlay overlay;
    return overlay;
}

LockscreenOverlay::~LockscreenOverlay()
{
    if (m_overlay) {
        kde_lockscreen_overlay_v1_destroy(m_overlay);
    }
}

void LockscreenOverlay::resolve()
{
    if (m_resolved) {
        return;
    }
    m_resolved = true;

    wl_display *display = waylandDisplay();
    if (!display) {
        m_reason = QStringLiteral("not running on Wayland");
        return;
    }

    wl_event_queue *queue = wl_display_create_queue(display);
    if (!queue) {
        m_reason = QStringLiteral("cannot create a Wayland queue");
        return;
    }

    auto *proxy = reinterpret_cast<wl_proxy *>(wl_display_get_registry(display));
    if (!proxy) {
        wl_event_queue_destroy(queue);
        m_reason = QStringLiteral("cannot read the Wayland registry");
        return;
    }
    wl_proxy_set_queue(proxy, queue);

    auto *registry = reinterpret_cast<wl_registry *>(proxy);
    RegistryHunt hunt{InterfaceName.data(), 0, 0, false};
    wl_registry_add_listener(registry, &RegistryListener, &hunt);
    wl_display_roundtrip_queue(display, queue);

    if (hunt.found) {
        auto *bound = static_cast<kde_lockscreen_overlay_v1 *>(
            wl_registry_bind(registry, hunt.name, &kde_lockscreen_overlay_v1_interface, 1));
        if (bound) {
            // Move it back to the default queue: the object is only ever used
            // to send requests from the main thread afterwards.
            wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(bound), nullptr);
            m_overlay = bound;
        }
    } else {
        m_reason = QStringLiteral("the compositor does not offer kde_lockscreen_overlay_v1");
    }

    wl_registry_destroy(registry);
    wl_event_queue_destroy(queue);
}

bool LockscreenOverlay::available()
{
    resolve();
    return m_overlay != nullptr;
}

bool LockscreenOverlay::allow(QWindow *window)
{
    if (!available()) {
        return false;
    }

    wl_surface *surface = waylandSurface(window);
    if (!surface) {
        m_reason = QStringLiteral("the window has no Wayland surface yet");
        return false;
    }

    // The protocol is explicit that this only counts while the surface is
    // unmapped, which is why callers have to ask before showing the window.
    kde_lockscreen_overlay_v1_allow(m_overlay, surface);
    wl_display_flush(waylandDisplay());
    return true;
}
