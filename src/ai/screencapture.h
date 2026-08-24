/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * One still picture of the screen, for the times a question is about what is
 * on it.
 *
 * The route is the desktop portal, which is the only way to ask for the screen
 * on Wayland without a compositor-specific hack, and which asks the user for
 * consent itself - Atoll deliberately does not try to work around that dialog.
 * Where no portal answers, Spectacle and grim are tried in turn so the feature
 * still works on a bare session.
 */
class ScreenCapture : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapture(QObject *parent = nullptr);

    /** Whether anything on this machine can take a screenshot at all. */
    static bool available();

    /**
     * Take one. `captured` carries PNG bytes already scaled down to something
     * a model will accept; `failed` carries a reason worth showing.
     */
    void capture();

Q_SIGNALS:
    void captured(const QByteArray &png);
    void failed(const QString &reason);

private Q_SLOTS:
    /** The portal's Request.Response, carrying a file URI on success. */
    void handlePortalResponse(uint response, const QVariantMap &results);

private:
    void viaPortal();
    void viaHelper();
    void deliver(const QString &path, bool temporary);

    int m_token = 0;
    bool m_portalTried = false;
};
