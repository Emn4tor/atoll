/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QRectF>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Config;
class QScreen;

/**
 * Turns ordinary QML `Window`s into wlr-layer-shell surfaces pinned to an edge
 * of a screen, and keeps their input regions glued to whatever the island
 * currently looks like.
 *
 * Each surface is a large transparent canvas: morphing a layer surface every
 * animation frame would make the compositor resize us at 60 Hz, so we claim a
 * generous rectangle once and only move the *input* region around. Everything
 * outside the island therefore stays click-through.
 *
 * One instance serves every screen the island was asked to appear on. `targets`
 * is the list QML instantiates windows from; it follows the config, the set of
 * connected outputs, and which output is currently the primary one.
 */
class ShellWindow : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.shell")

    Q_PROPERTY(bool layerShellAvailable READ layerShellAvailable CONSTANT)
    /** Output names to place an island on, resolved from island.screens. */
    Q_PROPERTY(QStringList targets READ targets NOTIFY targetsChanged)
    /** Every connected output, for the settings window to offer. */
    Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
    Q_PROPERTY(QString primaryScreen READ primaryScreen NOTIFY screensChanged)
    /** Whether the compositor lets surfaces stay up while the screen is locked. */
    Q_PROPERTY(bool lockScreenSupported READ lockScreenSupported CONSTANT)
    Q_PROPERTY(QString lockScreenReason READ lockScreenReason CONSTANT)

public:
    explicit ShellWindow(Config *config, QObject *parent = nullptr);

    bool layerShellAvailable() const;
    bool lockScreenSupported() const;
    QString lockScreenReason() const;
    QStringList targets() const
    {
        return m_targets;
    }
    QVariantList screens() const;
    QString primaryScreen() const;

    /** Apply layer-shell properties. Must run before the window is shown. */
    Q_INVOKABLE void configure(QQuickWindow *window, const QString &screenName);

    /**
     * The same, for a surface that covers a whole output rather than hugging an
     * edge: the assistant's edge glow. It takes no input at all and claims no
     * exclusive zone, so everything underneath keeps working while it is lit.
     */
    Q_INVOKABLE void configureOverlay(QQuickWindow *window, const QString &screenName);

    /**
     * Ask the compositor to keep this window up while the session is locked.
     * Must be called right after the window is shown and before it renders.
     */
    Q_INVOKABLE void allowOnLockScreen(QQuickWindow *window);

    /** Forget a window that is going away. */
    Q_INVOKABLE void release(QQuickWindow *window);

    /** The surface size claimed on one output. */
    Q_INVOKABLE int surfaceWidthFor(const QString &screenName) const;
    Q_INVOKABLE int surfaceHeightFor(const QString &screenName) const;

    /**
     * Grow the surface if the island has outgrown it. The configured height is
     * a floor, not a ceiling: a dashboard that is taller than the canvas it
     * morphs inside would simply be cut off.
     */
    Q_INVOKABLE void ensureSurfaceHeight(QQuickWindow *window, int height);

    /** Restrict pointer input to these rectangles (in window coordinates). */
    Q_INVOKABLE void setInputRegion(QQuickWindow *window, const QVariantList &rects);

    /** Ask the compositor for keyboard focus (used by the expanded dashboard). */
    Q_INVOKABLE void setKeyboardFocus(QQuickWindow *window, bool wanted);

Q_SIGNALS:
    void targetsChanged();
    void screensChanged();

private:
    QScreen *screenByName(const QString &name) const;
    QSize surfaceSizeFor(QScreen *screen, int minimumHeight = 0) const;
    void recomputeTargets();
    void watchScreens();
    void reapply();
    void reapplyOne(QQuickWindow *window, const QString &screenName);
    void reapplyOverlay(QQuickWindow *window, const QString &screenName);

    Config *m_config;
    QStringList m_targets;
    QHash<QQuickWindow *, QString> m_windows;
    QSet<QObject *> m_watched;
    QHash<QQuickWindow *, int> m_wanted;
    /** Full-output surfaces, which follow the screen but not the island. */
    QHash<QQuickWindow *, QString> m_overlays;
};
