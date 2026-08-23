/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

/**
 * A tiny in-memory image bank behind the `image://atoll/<key>` scheme.
 *
 * Notification hints hand us raw pixel buffers and MPRIS hands us remote
 * artwork; both end up here so QML can reference them by URL without touching
 * the filesystem.
 */
class ImageStore
{
public:
    static ImageStore &instance();

    /** Store (or replace) an image and return the URL QML should load. */
    QString put(const QString &key, const QImage &image);
    QImage get(const QString &key) const;
    void remove(const QString &key);
    void clear();

    /**
     * Perceptually-weighted dominant colour, used to tint the island after the
     * currently playing album art. Washed-out and near-black results are
     * rejected so the accent stays readable on a dark surface.
     */
    static QColor dominantColor(const QImage &image);

private:
    ImageStore() = default;
    mutable QMutex m_mutex;
    QHash<QString, QImage> m_images;
};

class ImageStoreProvider : public QQuickImageProvider
{
public:
    ImageStoreProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

/** Resolves `image://icon/<name>` against the active icon theme. */
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
