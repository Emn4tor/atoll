/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "share/sharecredentials.h"
#include "share/sharetypes.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Config;
class QNetworkAccessManager;
class ShareDiscovery;
class ShareSender;
class ShareServer;

/**
 * Files dropped on the island, and files other people drop on theirs.
 *
 * This is the piece QML talks to: one state machine ("staged" -> "sending" ->
 * "sent", or "incoming" -> "receiving" -> "received"), a list of the devices
 * currently within earshot, and the two verbs the island needs - offer these
 * files, send them to that device.
 */
class ShareService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.share")

    /** idle | staged | sending | sent | incoming | receiving | received | failed */
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY availabilityChanged)
    /** Whether incoming transfers can arrive at all. */
    Q_PROPERTY(bool listening READ listening NOTIFY availabilityChanged)
    Q_PROPERTY(QString alias READ alias NOTIFY availabilityChanged)
    /** Nearby devices, most recently heard from first. */
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    /** The files this transfer is about, as [{name, size}]. */
    Q_PROPERTY(QVariantList files READ files NOTIFY stateChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY stateChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY stateChanged)
    /** 0..1 through the current transfer. */
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    /** The device on the other end of the current transfer. */
    Q_PROPERTY(QString peer READ peer NOTIFY stateChanged)
    /** Why the last transfer failed, or where the received files went. */
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)

public:
    explicit ShareService(Config *config, QObject *parent = nullptr);
    ~ShareService() override;

    void start();

    QString state() const
    {
        return m_state;
    }
    bool enabled() const;
    bool listening() const;
    QString alias() const
    {
        return m_identity.alias;
    }
    QVariantList devices() const;
    QVariantList files() const
    {
        return m_summary;
    }
    int fileCount() const
    {
        return int(m_summary.size());
    }
    qint64 totalBytes() const
    {
        return m_total;
    }
    qreal progress() const;
    QString peer() const
    {
        return m_peerAlias;
    }
    QString message() const
    {
        return m_message;
    }

    /** Stage what was dropped on the island and go looking for devices. */
    Q_INVOKABLE void offer(const QList<QUrl> &urls);
    /** The same, for callers that only have paths (D-Bus, the CLI). */
    Q_INVOKABLE void offerPaths(const QStringList &paths);
    /** Ask everyone nearby to say who they are. */
    Q_INVOKABLE void probe();
    Q_INVOKABLE void sendTo(const QString &fingerprint);
    /** Accept or decline the incoming transfer the island is showing. */
    Q_INVOKABLE void respond(bool accept);
    /** Abandon whatever is going on and go back to resting. */
    Q_INVOKABLE void dismiss();
    /** Open the folder the last transfer landed in. */
    Q_INVOKABLE void openDestination() const;

Q_SIGNALS:
    void stateChanged();
    void devicesChanged();
    void progressChanged();
    void availabilityChanged();

private:
    void configure();
    void setState(const QString &state, const QString &message = {});
    void remember(const SharePeer &peer);
    /** Drop devices that did not answer the last time we called out. */
    void forgetSilentPeers();
    void forget(const QString &fingerprint);
    void answer(const SharePeer &peer);
    void collect(const QString &path, const QString &relativeTo, QList<ShareFile> *into) const;
    QString destination() const;
    static QString dataDirectory();
    static QString persistentFingerprint();

    Config *m_config = nullptr;
    ShareDiscovery *m_discovery = nullptr;
    ShareServer *m_server = nullptr;
    ShareSender *m_sender = nullptr;
    QNetworkAccessManager *m_network = nullptr;

    ShareIdentity m_identity;
    ShareCredentials m_credentials;
    QList<SharePeer> m_peers;
    QList<ShareFile> m_staged;
    QVariantList m_summary;

    QString m_state = QStringLiteral("idle");
    QString m_message;
    QString m_peerAlias;
    QString m_peerFingerprint;
    QDateTime m_probedAt;
    qint64 m_total = 0;
    qint64 m_done = 0;
    QTimer m_reset;
    QTimer m_probe;
    QTimer m_sweep;
    /** The port that was asked for, which is not always the one we got. */
    quint16 m_appliedPort = 0;
    bool m_started = false;
};
