/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QString>

class QWindow;

/**
 * Permission to stay on screen while the session is locked.
 *
 * KWin exposes `kde_lockscreen_overlay_v1` for surfaces that have a reason to
 * outlive the lock - alarms, calls, and, if the user asks for it, the island.
 * The request only counts while the surface is still unmapped, so it has to be
 * made between creating a window and showing it.
 *
 * Everything here degrades quietly: on a compositor without the protocol, or
 * outside Wayland, `allow()` does nothing and the island simply disappears
 * with the rest of the session when the screen locks.
 */
class LockscreenOverlay
{
public:
    static LockscreenOverlay &instance();

    /** Whether the compositor offers the protocol at all. */
    bool available();

    /**
     * Ask for this window to survive the lock screen. Must be called after the
     * window has a surface (QWindow::create()) and before it is shown.
     */
    bool allow(QWindow *window);

    /** Why the last call did nothing, for the settings window to explain. */
    QString unavailableReason() const
    {
        return m_reason;
    }

private:
    LockscreenOverlay() = default;
    ~LockscreenOverlay();
    LockscreenOverlay(const LockscreenOverlay &) = delete;
    LockscreenOverlay &operator=(const LockscreenOverlay &) = delete;

    void resolve();

    bool m_resolved = false;
    QString m_reason;
    struct kde_lockscreen_overlay_v1 *m_overlay = nullptr;
};
