/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QByteArray>
#include <QObject>
#include <QRect>
#include <QString>
#include <QVariantList>

/**
 * One still picture of the screen, for the times a question is about what is
 * on it.
 *
 * The route is the desktop portal, which is the only way to ask for the screen
 * on Wayland without a compositor-specific hack, and which asks the user for
 * consent itself - Atoll deliberately does not try to work around that dialog.
 * Where no portal answers, Spectacle and grim are tried in turn so the feature
 * still works on a bare session.
 *
 * A picture can be of one output rather than of everything. That is not a
 * convenience: three monitors side by side make a 5760-pixel-wide image, and
 * everything a model is given is scaled to fit inside a box about 1568 pixels
 * across, so asking about all of them at once is asking about a picture in
 * which no window title is legible any more. One screen fills that box.
 */
class ScreenCapture : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCapture(QObject *parent = nullptr);

    /** Whether anything on this machine can take a screenshot at all. */
    static bool available();

    /**
     * The outputs a picture can be taken of, as
     * `{name, label, width, height, primary}` - `width` and `height` in real
     * pixels, because that is what decides how much of it survives scaling.
     */
    static QVariantList outputs();

    /** Whether there is more than one output to choose between. */
    static bool hasSeveralOutputs();

    /** The output the pointer is on, falling back to the primary one. */
    static QString currentOutputName();

    /** A name a person recognises: "DP-1 - LG 27\"" rather than "DP-1". */
    static QString labelFor(const QString &screenName);

    /** True if an output by that name exists right now. */
    static bool hasOutput(const QString &screenName);

    /**
     * The longest edge of the delivered picture, in pixels. Bigger than the
     * provider's own ceiling only wastes bytes; smaller loses text.
     */
    void setMaxEdge(int pixels);

    /**
     * Take one. `screenName` empty, or "all", means the whole desktop; any
     * other name means that output alone. `captured` carries PNG bytes already
     * scaled down to something a model will accept; `failed` carries a reason
     * worth showing.
     */
    void capture(const QString &screenName = {});

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

    /** Whether the current request is for one output rather than for all. */
    bool wantsOneScreen() const;
    /**
     * Where the wanted output sits inside a picture of the whole desktop, as
     * fractions of it. The layout is known in logical coordinates and the
     * picture is in real pixels, so the mapping is proportional rather than
     * arithmetic - which is also what keeps it right when outputs are scaled
     * differently from each other.
     */
    QRectF fractionOfDesktop() const;

    int m_token = 0;
    bool m_portalTried = false;
    /** grim is tried once per request; on KDE it never works. */
    bool m_grimTried = false;
    /** True when the picture in hand is already the one output that was asked for. */
    bool m_alreadyCut = false;
    /** The output asked for; empty means the whole desktop. */
    QString m_screen;
    /** What the providers accept without re-encoding it themselves. */
    int m_maxEdge = 1568;
};
