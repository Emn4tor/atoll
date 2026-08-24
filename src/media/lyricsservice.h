/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Config;
class MprisManager;
class MprisPlayer;
class QNetworkAccessManager;
class QNetworkReply;

/**
 * Lyrics for whatever the island is currently showing.
 *
 * Three sources, in order: an `.lrc` file sitting next to a local track, the
 * on-disk cache, and lrclib.net. The remote lookup is the only thing Atoll
 * ever sends off the machine, so it is one switch away from being off, and it
 * only ever carries artist, title, album and duration.
 *
 * Synced lyrics are kept as a flat list of (time, text) pairs; `currentIndex`
 * follows the player's own position, which is what makes the island able to
 * show the line being sung right now.
 */
class LyricsService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.lyrics")

    /** idle | loading | synced | plain | missing | error | disabled */
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    /** [{ time: ms, text: string }], empty for unsynced or missing lyrics. */
    Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(bool synced READ synced NOTIFY linesChanged)
    Q_PROPERTY(QString plain READ plain NOTIFY linesChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentLine READ currentLine NOTIFY currentIndexChanged)
    Q_PROPERTY(QString nextLine READ nextLine NOTIFY currentIndexChanged)
    /** Where the current lyrics came from: file | cache | lrclib. */
    Q_PROPERTY(QString origin READ origin NOTIFY linesChanged)
    Q_PROPERTY(QString trackLabel READ trackLabel NOTIFY linesChanged)

public:
    explicit LyricsService(Config *config, MprisManager *media, QObject *parent = nullptr);

    QString state() const
    {
        return m_state;
    }
    bool enabled() const;
    QVariantList lines() const
    {
        return m_lines;
    }
    bool synced() const
    {
        return !m_lines.isEmpty();
    }
    QString plain() const
    {
        return m_plain;
    }
    int currentIndex() const
    {
        return m_currentIndex;
    }
    QString currentLine() const;
    QString nextLine() const;
    QString origin() const
    {
        return m_origin;
    }
    QString trackLabel() const
    {
        return m_trackLabel;
    }

    /** Drop the cached copy for this track and look it up again. */
    Q_INVOKABLE void refresh();

    /** Playback position of a line, in microseconds, for seeking. */
    Q_INVOKABLE qint64 positionOf(int index) const;

Q_SIGNALS:
    void stateChanged();
    void linesChanged();
    void currentIndexChanged();

private:
    void onTrackChanged();
    void lookup(bool bypassCache);
    void requestRemote(bool search);
    void handleReply(const QString &payload, bool wasSearch);
    void applyLrc(const QString &lrc, const QString &origin);
    void applyPlain(const QString &text, const QString &origin);
    void publishState(const QString &state);
    void clearLines();
    void updateCurrentIndex();

    QString cachePath() const;
    QString missKey() const;
    QString localCandidate() const;
    static QVariantList parseLrc(const QString &lrc);

    Config *m_config;
    MprisManager *m_media;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QPointer<MprisPlayer> m_player;

    QTimer m_debounce;
    QString m_state = QStringLiteral("idle");
    QString m_origin;
    QString m_trackLabel;
    QString m_plain;
    QVariantList m_lines;
    int m_currentIndex = -1;
    bool m_wasEnabled = false;

    QString m_artist;
    QString m_title;
    QString m_album;
    qint64 m_length = 0;
    /** Tracks lrclib had nothing for, so we ask once per track, not per seek. */
    QSet<QString> m_misses;
};
