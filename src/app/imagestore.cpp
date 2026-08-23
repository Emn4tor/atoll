/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "imagestore.h"

#include <QFileInfo>
#include <QIcon>
#include <QMutexLocker>
#include <QPainter>

using namespace Qt::StringLiterals;

ImageStore &ImageStore::instance()
{
    static ImageStore store;
    return store;
}

QString ImageStore::put(const QString &key, const QImage &image)
{
    {
        QMutexLocker lock(&m_mutex);
        m_images.insert(key, image);
    }
    // The revision suffix busts QML's image cache when a key is reused.
    return u"image://atoll/%1"_s.arg(key);
}

QImage ImageStore::get(const QString &key) const
{
    QMutexLocker lock(&m_mutex);
    return m_images.value(key);
}

void ImageStore::remove(const QString &key)
{
    QMutexLocker lock(&m_mutex);
    m_images.remove(key);
}

void ImageStore::clear()
{
    QMutexLocker lock(&m_mutex);
    m_images.clear();
}

QColor ImageStore::dominantColor(const QImage &image)
{
    if (image.isNull()) {
        return {};
    }

    // Downscale hard: we want the mood of the cover, not its detail.
    const QImage small = image.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_ARGB32);

    // Bucket by hue, weighting each pixel by saturation so a small splash of
    // colour beats a large grey field.
    constexpr int Buckets = 24;
    double weight[Buckets] = {};
    double sumS[Buckets] = {};
    double sumV[Buckets] = {};

    double fallbackV = 0.0;
    int fallbackCount = 0;

    for (int y = 0; y < small.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const QColor c = QColor::fromRgb(line[x]);
            if (qAlpha(line[x]) < 128) {
                continue;
            }
            int h = 0;
            int s = 0;
            int v = 0;
            c.getHsv(&h, &s, &v);
            fallbackV += v;
            ++fallbackCount;
            if (h < 0 || s < 40 || v < 40) {
                continue;
            }
            const int bucket = (h * Buckets / 360) % Buckets;
            const double w = (s / 255.0) * (v / 255.0);
            weight[bucket] += w;
            sumS[bucket] += s * w;
            sumV[bucket] += v * w;
        }
    }

    int best = -1;
    for (int i = 0; i < Buckets; ++i) {
        if (best < 0 || weight[i] > weight[best]) {
            best = i;
        }
    }

    if (best < 0 || weight[best] <= 0.0) {
        // Monochrome cover: return a neutral derived from its brightness.
        const int v = fallbackCount ? qBound(90, int(fallbackV / fallbackCount), 220) : 160;
        return QColor::fromHsv(0, 0, v);
    }

    const int hue = (best * 360 / Buckets) + (360 / Buckets / 2);
    int sat = qBound(90, int(sumS[best] / weight[best]), 235);
    int val = qBound(150, int(sumV[best] / weight[best]), 255);

    return QColor::fromHsv(hue % 360, sat, val);
}

QImage ImageStoreProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // Keys may carry a "?v=N" cache-buster; strip it before lookup.
    const QString key = id.section(u'?', 0, 0);
    QImage image = ImageStore::instance().get(key);
    if (image.isNull()) {
        return {};
    }
    if (requestedSize.isValid() && !requestedSize.isEmpty()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (size) {
        *size = image.size();
    }
    return image;
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QStringList candidates = id.split(u',', Qt::SkipEmptyParts);
    const int edge = requestedSize.width() > 0 ? requestedSize.width() : 48;

    QIcon icon;
    for (const QString &name : candidates) {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        // Some apps pass an absolute path or a file:// URL as their icon.
        if (trimmed.startsWith(u'/') && QFileInfo::exists(trimmed)) {
            icon = QIcon(trimmed);
        } else {
            icon = QIcon::fromTheme(trimmed);
        }
        if (!icon.isNull() && !icon.availableSizes().isEmpty()) {
            break;
        }
        if (!icon.isNull() && icon.actualSize(QSize(edge, edge)).isValid()) {
            break;
        }
    }

    QPixmap pixmap = icon.isNull() ? QPixmap() : icon.pixmap(QSize(edge, edge));
    if (pixmap.isNull()) {
        pixmap = QPixmap(edge, edge);
        pixmap.fill(Qt::transparent);
    }
    if (size) {
        *size = pixmap.size();
    }
    return pixmap;
}
