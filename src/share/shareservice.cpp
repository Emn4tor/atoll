/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "share/shareservice.h"

#include "config/config.h"
#include "share/sharediscovery.h"
#include "share/sharesender.h"
#include "share/shareserver.h"

#include <algorithm>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace
{
/** How long a device stays in the list after it was last heard from. */
constexpr qint64 PeerLifetimeSeconds = 5 * 60;
/** How long a device gets to answer a call-out before it is written off. */
constexpr int SilenceGrace = 5'000;
/** How long a finished transfer stays on the island before it packs up. */
constexpr int ResultLinger = 5'000;
/** How long a device list waits for a click before giving the island back. */
constexpr int StagedLinger = 45'000;

QString defaultAlias()
{
    const QString host = QSysInfo::machineHostName();
    return host.isEmpty() ? u"Atoll"_s : host;
}
}

ShareService::ShareService(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_discovery(new ShareDiscovery(this))
    , m_server(new ShareServer(this))
    , m_sender(new ShareSender(this))
    , m_network(new QNetworkAccessManager(this))
{
    m_reset.setSingleShot(true);
    connect(&m_reset, &QTimer::timeout, this, &ShareService::dismiss);

    // While the island is showing a device list, keep asking: a phone that
    // joins the network mid-drag should still turn up without a second try.
    m_probe.setInterval(3'000);
    connect(&m_probe, &QTimer::timeout, this, [this] {
        if (m_state == u"staged"_s) {
            m_discovery->announce(true);
        } else {
            m_probe.stop();
        }
    });

    // A device that was here a minute ago may be in a bag by now. Every time
    // the island calls out, whoever does not answer stops being offered.
    m_sweep.setSingleShot(true);
    m_sweep.setInterval(SilenceGrace);
    connect(&m_sweep, &QTimer::timeout, this, &ShareService::forgetSilentPeers);

    connect(m_discovery, &ShareDiscovery::peerSeen, this, &ShareService::remember);
    connect(m_discovery, &ShareDiscovery::replyWanted, this, &ShareService::answer);

    connect(m_server, &ShareServer::peerSeen, this, &ShareService::remember);
    connect(m_server, &ShareServer::approvalRequested, this,
            [this](const SharePeer &peer, const QVariantList &files, qint64 total) {
                remember(peer);
                m_summary = files;
                m_total = total;
                m_done = 0;
                m_peerAlias = peer.alias;
                setState(u"incoming"_s);
            });
    connect(m_server, &ShareServer::receiveProgress, this, [this](qint64 received, qint64 total) {
        m_done = received;
        m_total = total;
        Q_EMIT progressChanged();
    });
    connect(m_server, &ShareServer::receiveFinished, this, [this](const QStringList &paths) {
        m_done = m_total;
        setState(u"received"_s, paths.size() == 1 ? QFileInfo(paths.first()).fileName() : destination());
    });
    connect(m_server, &ShareServer::receiveFailed, this, [this](const QString &why) {
        setState(u"failed"_s, why);
    });

    connect(m_sender, &ShareSender::progress, this, [this](qint64 sent, qint64 total) {
        m_done = sent;
        m_total = total;
        Q_EMIT progressChanged();
    });
    connect(m_sender, &ShareSender::fileChanged, this, &ShareService::stateChanged);
    connect(m_sender, &ShareSender::finished, this, [this] {
        setState(u"sent"_s, m_peerAlias);
    });
    connect(m_sender, &ShareSender::failed, this, [this](const QString &why) {
        // Whatever went wrong, that device has just proven it cannot take the
        // files. Take it off the list rather than offering it again; if it is
        // still out there it answers the next call-out and comes straight back.
        forget(m_peerFingerprint);
        if (!m_staged.isEmpty()) {
            // The files are still here, so hand the picker back with the
            // reason on it instead of throwing the drop away.
            m_done = 0;
            setState(u"staged"_s, why);
            probe();
            return;
        }
        setState(u"failed"_s, why);
    });

    connect(m_config, &Config::changed, this, &ShareService::configure);
}

ShareService::~ShareService() = default;

bool ShareService::enabled() const
{
    return m_config->value(u"modules.sharing"_s, true).toBool();
}

bool ShareService::listening() const
{
    return m_server->isListening();
}

QString ShareService::destination() const
{
    const QString configured = m_config->value(u"sharing.saveDirectory"_s).toString().trimmed();
    if (!configured.isEmpty()) {
        QString expanded = configured;
        if (expanded.startsWith(u'~')) {
            expanded.replace(0, 1, QDir::homePath());
        }
        return expanded;
    }
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return downloads.isEmpty() ? QDir::homePath() : downloads;
}

/**
 * The fingerprint is how other devices tell "this machine again" from "someone
 * new", so it has to outlive the process. It is not a secret and not a key -
 * Atoll's own server is cleartext - which is why a random string in a file is
 * the whole of it.
 */
QString ShareService::dataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/atoll"_s;
}

QString ShareService::persistentFingerprint()
{
    const QString directory = dataDirectory();
    const QString path = directory + u"/fingerprint"_s;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QString stored = QString::fromLatin1(file.readAll()).trimmed();
        if (!stored.isEmpty()) {
            return stored;
        }
    }

    const QString fresh = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(directory);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(fresh.toLatin1());
    }
    return fresh;
}

void ShareService::start()
{
    m_started = true;
    // Making the certificate is a one-off, and it decides whether this machine
    // announces itself as an encrypted device or a cleartext one.
    m_credentials = ShareCredentials::load(dataDirectory());
    m_server->setCredentials(m_credentials);
    m_sender->setCredentials(m_credentials);
    configure();
}

void ShareService::configure()
{
    if (!m_started) {
        return;
    }

    const QString configuredAlias = m_config->value(u"sharing.alias"_s).toString().trimmed();
    const quint16 wanted = quint16(m_config->value(u"sharing.port"_s, 53317).toInt());
    const bool receive = m_config->value(u"sharing.receive"_s, true).toBool();

    if (!enabled()) {
        if (m_discovery->isRunning() || m_server->isListening()) {
            dismiss();
            m_discovery->stop();
            m_server->close();
            m_appliedPort = 0;
            Q_EMIT availabilityChanged();
        }
        return;
    }

    m_server->setAutoAccept(m_config->value(u"sharing.autoAccept"_s, false).toBool());
    m_server->setDestination(destination());

    // Moving the port is a restart of both halves, and only of both halves:
    // the port Atoll ended up on may differ from the one that was asked for.
    if (m_appliedPort != 0 && wanted != m_appliedPort) {
        dismiss();
        m_server->close();
        m_discovery->stop();
    }
    m_appliedPort = wanted;

    const bool wasListening = m_server->isListening();
    if (receive && !wasListening) {
        m_identity.port = m_server->listenOn(wanted);
    } else if (!receive && wasListening) {
        m_server->close();
    }
    if (!receive || !m_server->isListening()) {
        // Without a server there is nowhere for a reply to land, so peers are
        // told to talk to the port anyway; sending still works either way.
        m_identity.port = wanted;
    }

    m_identity.alias = configuredAlias.isEmpty() ? defaultAlias() : configuredAlias;
    m_identity.deviceModel = QSysInfo::prettyProductName();
    m_identity.deviceType = u"desktop"_s;
    // In TLS mode the certificate hash *is* the identity; without one, any
    // stable random string will do, because nothing can be verified anyway.
    m_identity.protocol = m_credentials.isValid() ? u"https"_s : u"http"_s;
    m_identity.fingerprint =
        m_credentials.isValid() ? m_credentials.fingerprint : persistentFingerprint();

    m_discovery->setIdentity(m_identity);
    m_server->setIdentity(m_identity);
    m_sender->setIdentity(m_identity);

    if (!m_discovery->isRunning()) {
        const QHostAddress group(m_config->value(u"sharing.multicast"_s, u"224.0.0.167"_s).toString());
        if (m_discovery->start(group.isNull() ? QHostAddress(u"224.0.0.167"_s) : group, wanted)) {
            m_discovery->announce(true);
        }
    }

    Q_EMIT availabilityChanged();
}

QVariantList ShareService::devices() const
{
    QVariantList list;
    list.reserve(m_peers.size());
    for (const SharePeer &peer : m_peers) {
        list.append(peer.toVariantMap());
    }
    return list;
}

qreal ShareService::progress() const
{
    if (m_total <= 0) {
        return 0;
    }
    return qBound<qreal>(0, qreal(m_done) / qreal(m_total), 1);
}

void ShareService::remember(const SharePeer &peer)
{
    if (!peer.isValid() || peer.fingerprint == m_identity.fingerprint) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    SharePeer updated = peer;
    updated.seen = now;

    bool changed = true;
    for (qsizetype i = 0; i < m_peers.size(); ++i) {
        if (m_peers.at(i).fingerprint == peer.fingerprint) {
            changed = m_peers.at(i).alias != peer.alias || m_peers.at(i).address != peer.address
                      || m_peers.at(i).port != peer.port;
            m_peers.removeAt(i);
            break;
        }
    }
    m_peers.prepend(updated);

    const auto stale = std::remove_if(m_peers.begin(), m_peers.end(), [&now](const SharePeer &known) {
        return known.seen.secsTo(now) > PeerLifetimeSeconds;
    });
    if (stale != m_peers.end()) {
        m_peers.erase(stale, m_peers.end());
        changed = true;
    }

    if (changed) {
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: share sees %s at %s (%lld nearby)", qUtf8Printable(updated.alias),
                     qUtf8Printable(updated.baseUrl()), qint64(m_peers.size()));
        }
        Q_EMIT devicesChanged();
    }
}

void ShareService::answer(const SharePeer &peer)
{
    // Tell that one device who we are. The multicast fallback in the protocol
    // covers the case where it cannot be reached over HTTP.
    QNetworkRequest request{QUrl(peer.baseUrl() + u"/register"_s)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    request.setTransferTimeout(5'000);
    if (peer.protocol == u"https"_s) {
        QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
        tls.setPeerVerifyMode(QSslSocket::VerifyNone);
        if (m_credentials.isValid()) {
            tls.setLocalCertificate(m_credentials.certificate);
            tls.setPrivateKey(m_credentials.key);
        }
        request.setSslConfiguration(tls);
    }

    QNetworkReply *reply =
        m_network->post(request, QJsonDocument(m_identity.toJson()).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::sslErrors, reply, [reply] {
        reply->ignoreSslErrors();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_discovery->announce(false);
        }
    });
}

void ShareService::collect(const QString &path, const QString &relativeTo, QList<ShareFile> *into) const
{
    static QMimeDatabase mimeDatabase;

    const QFileInfo info(path);
    if (info.isDir()) {
        // A dropped folder keeps its shape: the protocol carries a relative
        // path in the file name, and receivers recreate the tree from it.
        QDirIterator iterator(path, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            collect(iterator.next(), relativeTo, into);
        }
        return;
    }
    if (!info.isFile() || !info.isReadable()) {
        return;
    }

    ShareFile file;
    file.id = QString::number(into->size());
    file.path = info.absoluteFilePath();
    file.name = relativeTo.isEmpty() ? info.fileName() : QDir(relativeTo).relativeFilePath(file.path);
    file.size = info.size();
    file.mimeType = mimeDatabase.mimeTypeForFile(info).name();
    into->append(file);
}

void ShareService::offer(const QList<QUrl> &urls)
{
    if (!enabled()) {
        return;
    }

    m_sender->cancel();
    m_reset.stop();

    QList<ShareFile> staged;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        const QFileInfo info(path);
        collect(path, info.isDir() ? info.absolutePath() : QString(), &staged);
    }

    if (staged.isEmpty()) {
        setState(u"failed"_s, tr("Only local files can be shared."));
        return;
    }

    m_staged = staged;
    m_summary.clear();
    m_total = 0;
    m_done = 0;
    for (const ShareFile &file : std::as_const(m_staged)) {
        m_summary.append(QVariantMap{{u"name"_s, file.name}, {u"size"_s, file.size}});
        m_total += file.size;
    }

    m_peerAlias.clear();
    setState(u"staged"_s);
    probe();
}

void ShareService::offerPaths(const QStringList &paths)
{
    QList<QUrl> urls;
    urls.reserve(paths.size());
    for (const QString &path : paths) {
        urls.append(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
    }
    offer(urls);
}

void ShareService::probe()
{
    if (!enabled()) {
        return;
    }
    if (!m_discovery->isRunning()) {
        configure();
    }
    m_probedAt = QDateTime::currentDateTimeUtc();
    m_discovery->announce(true);
    m_probe.start();
    m_sweep.start();
}

void ShareService::forgetSilentPeers()
{
    if (m_probedAt.isNull()) {
        return;
    }

    const auto silent = std::remove_if(m_peers.begin(), m_peers.end(), [this](const SharePeer &peer) {
        return peer.seen < m_probedAt;
    });
    if (silent == m_peers.end()) {
        return;
    }
    m_peers.erase(silent, m_peers.end());
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: share dropped silent device(s), %lld nearby", qint64(m_peers.size()));
    }
    Q_EMIT devicesChanged();
}

void ShareService::forget(const QString &fingerprint)
{
    if (fingerprint.isEmpty()) {
        return;
    }
    const auto gone = std::remove_if(m_peers.begin(), m_peers.end(), [&fingerprint](const SharePeer &peer) {
        return peer.fingerprint == fingerprint;
    });
    if (gone == m_peers.end()) {
        return;
    }
    m_peers.erase(gone, m_peers.end());
    Q_EMIT devicesChanged();
}

void ShareService::sendTo(const QString &fingerprint)
{
    if (m_staged.isEmpty()) {
        return;
    }

    const auto match = std::find_if(m_peers.cbegin(), m_peers.cend(), [&fingerprint](const SharePeer &peer) {
        return peer.fingerprint == fingerprint;
    });
    if (match == m_peers.cend()) {
        setState(u"failed"_s, tr("That device is no longer reachable."));
        return;
    }

    m_probe.stop();
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: sending %lld file(s) to %s", qint64(m_staged.size()),
                 qUtf8Printable(match->baseUrl()));
    }
    m_peerAlias = match->alias;
    m_peerFingerprint = match->fingerprint;
    m_done = 0;
    setState(u"sending"_s);
    m_sender->send(*match, m_staged);
}

void ShareService::respond(bool accept)
{
    if (m_state != u"incoming"_s) {
        return;
    }
    m_server->resolveApproval(accept);
    if (accept) {
        setState(u"receiving"_s);
    } else {
        m_summary.clear();
        m_total = 0;
        setState(u"idle"_s);
    }
}

void ShareService::dismiss()
{
    m_reset.stop();
    m_probe.stop();
    m_sweep.stop();
    m_sender->cancel();
    m_server->abortSession();

    m_staged.clear();
    m_summary.clear();
    m_total = 0;
    m_done = 0;
    m_peerAlias.clear();
    setState(u"idle"_s);
}

void ShareService::openDestination() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(destination()));
}

void ShareService::setState(const QString &state, const QString &message)
{
    const bool same = state == m_state && message == m_message;
    m_state = state;
    m_message = message;

    // A result is a message, not a mode: it shows for a moment and then the
    // island goes back to whatever it was doing. A device list is patient, but
    // not forever - files nobody sent should not hold the island hostage.
    if (state == u"sent"_s || state == u"received"_s || state == u"failed"_s) {
        m_reset.start(ResultLinger);
    } else if (state == u"staged"_s) {
        m_reset.start(StagedLinger);
    }

    if (!same) {
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: share -> %s %s", qUtf8Printable(m_state), qUtf8Printable(m_message));
        }
        Q_EMIT stateChanged();
        Q_EMIT progressChanged();
    }
}
