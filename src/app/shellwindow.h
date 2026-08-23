/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QRectF>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Config;
class QScreen;

/**
 * Turns an ordinary QML `Window` into a wlr-layer-shell surface pinned to the
 * top of the screen, and keeps its input region glued to whatever the island
 * currently looks like.
 *
 * The surface itself is a large transparent canvas: morphing a layer surface
 * every animation frame would make the compositor resize us at 60 Hz, so we
 * claim a generous rectangle once and only move the *input* region around.
 * Everything outside the island therefore stays click-through.
 */
class ShellWindow : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.shell")

    Q_PROPERTY(bool layerShellAvailable READ layerShellAvailable CONSTANT)
    Q_PROPERTY(int surfaceWidth READ surfaceWidth NOTIFY surfaceSizeChanged)
    Q_PROPERTY(int surfaceHeight READ surfaceHeight NOTIFY surfaceSizeChanged)

public:
    explicit ShellWindow(Config *config, QObject *parent = nullptr);

    bool layerShellAvailable() const;
    int surfaceWidth() const
    {
        return m_surfaceSize.width();
    }
    int surfaceHeight() const
    {
        return m_surfaceSize.height();
    }

    /** Apply layer-shell properties. Must run before the window is shown. */
    Q_INVOKABLE void configure(QQuickWindow *window);

    /** Restrict pointer input to these rectangles (in window coordinates). */
    Q_INVOKABLE void setInputRegion(QQuickWindow *window, const QVariantList &rects);

    /** Ask the compositor for keyboard focus (used by the expanded dashboard). */
    Q_INVOKABLE void setKeyboardFocus(QQuickWindow *window, bool wanted);

Q_SIGNALS:
    void surfaceSizeChanged();

private:
    QScreen *targetScreen() const;
    void recomputeSurfaceSize();
    void reapply();

    Config *m_config;
    QQuickWindow *m_window = nullptr;
    QSize m_surfaceSize{1200, 520};
};
