/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lyricsservice.h"

#include "config/config.h"
#include "dbus/mprismanager.h"
#include "dbus/mprisplayer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#include <limits>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView Endpoint{"https://lrclib.net/api/"};

QString sanitise(const QString &value)
{
    // Spotify and friends decorate titles with remaster and feature tags that
    // no lyrics database has ever heard of.
    static const QRegularExpression noise(
        uR"(\s*[\(\[][^\)\]]*(remaster|remastered|live|deluxe|mono|stereo|version|edit|feat\.?|ft\.?)[^\)\]]*[\)\]])"_s,
        QRegularExpression::CaseInsensitiveOption);
    QString cleaned = value;
    cleaned.remove(noise);
    static const QRegularExpression dashed(uR"(\s+-\s+(remaster|remastered)[^-]*$)"_s,
                                           QRegularExpression::CaseInsensitiveOption);
    cleaned.remove(dashed);
    return cleaned.simplified();
}
}

LyricsService::LyricsService(Config *config, MprisManager *media, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_media(media)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(400);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        lookup(false);
    });

    connect(m_media, &MprisManager::activeChanged, this, &LyricsService::onTrackChanged);
    connect(m_media, &MprisManager::trackChanged, this, &LyricsService::onTrackChanged);
    m_wasEnabled = enabled();
    connect(m_config, &Config::changed, this, [this] {
        Q_EMIT stateChanged();
        // Only the switch itself is worth reacting to. Every other key lands
        // here too - a slider being dragged in the settings window writes the
        // file continuously - and none of them should cost a lookup.
        const bool nowEnabled = enabled();
        if (nowEnabled != m_wasEnabled) {
            m_wasEnabled = nowEnabled;
            if (!nowEnabled) {
                clearLines();
                publishState(u"disabled"_s);
            } else {
                m_title.clear(); // Force the next lookup to run.
                onTrackChanged();
            }
        }
        updateCurrentIndex();
    });

    onTrackChanged();
}

bool LyricsService::enabled() const
{
    return m_config->value(u"lyrics.enabled"_s, true).toBool() && m_config->value(u"modules.lyrics"_s, true).toBool();
}

QString LyricsService::currentLine() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_lines.size()) {
        return {};
    }
    return m_lines.at(m_currentIndex).toMap().value(u"text"_s).toString();
}

QString LyricsService::nextLine() const
{
    const int next = m_currentIndex + 1;
    if (next <= 0 || next >= m_lines.size()) {
        return {};
    }
    return m_lines.at(next).toMap().value(u"text"_s).toString();
}

qint64 LyricsService::positionOf(int index) const
{
    if (index < 0 || index >= m_lines.size()) {
        return 0;
    }
    const qint64 ms = m_lines.at(index).toMap().value(u"time"_s).toLongLong();
    return qMax<qint64>(0, ms - m_config->value(u"lyrics.offsetMs"_s, 0).toInt()) * 1000;
}

void LyricsService::onTrackChanged()
{
    MprisPlayer *player = m_media->active();
    if (player != m_player) {
        if (m_player) {
            disconnect(m_player, nullptr, this, nullptr);
        }
        m_player = player;
        if (m_player) {
            connect(m_player, &MprisPlayer::metadataChanged, this, &LyricsService::onTrackChanged);
            connect(m_player, &MprisPlayer::positionChanged, this, &LyricsService::updateCurrentIndex);
        }
    }

    const QString artist = m_player ? m_player->artist() : QString();
    const QString title = m_player ? m_player->title() : QString();
    const QString album = m_player ? m_player->album() : QString();
    const qint64 length = m_player ? m_player->length() : 0;

    if (artist == m_artist && title == m_title && album == m_album && length == m_length) {
        return;
    }

    m_artist = artist;
    m_title = title;
    m_album = album;
    m_length = length;

    clearLines();

    if (!enabled()) {
        publishState(u"disabled"_s);
        return;
    }
    if (m_title.isEmpty()) {
        publishState(u"idle"_s);
        return;
    }

    publishState(u"loading"_s);
    m_debounce.start();
}

void LyricsService::refresh()
{
    const QString path = cachePath();
    if (!path.isEmpty()) {
        QFile::remove(path);
    }
    m_misses.remove(missKey());
    lookup(true);
}

void LyricsService::clearLines()
{
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }
    const bool had = !m_lines.isEmpty() || !m_plain.isEmpty();
    m_lines.clear();
    m_plain.clear();
    m_origin.clear();
    m_trackLabel.clear();
    if (m_currentIndex != -1) {
        m_currentIndex = -1;
        Q_EMIT currentIndexChanged();
    }
    if (had) {
        Q_EMIT linesChanged();
    }
}

void LyricsService::publishState(const QString &state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: lyrics %s for \"%s\" (%lld lines, from %s)",
                 qUtf8Printable(state),
                 qUtf8Printable(m_trackLabel.isEmpty() ? m_title : m_trackLabel),
                 qint64(m_lines.size()),
                 qUtf8Printable(m_origin.isEmpty() ? u"-"_s : m_origin));
    }
    Q_EMIT stateChanged();
}

QString LyricsService::missKey() const
{
    return m_artist + u"|"_s + m_title;
}

QString LyricsService::localCandidate() const
{
    if (!m_player) {
        return {};
    }
    const QUrl url(m_player->url());
    if (!url.isLocalFile()) {
        return {};
    }
    const QFileInfo info(url.toLocalFile());
    const QString sibling = info.absolutePath() + u"/"_s + info.completeBaseName() + u".lrc"_s;
    return QFile::exists(sibling) ? sibling : QString();
}

QString LyricsService::cachePath() const
{
    if (!m_config->value(u"lyrics.cache"_s, true).toBool() || m_title.isEmpty()) {
        return {};
    }
    const QString identity = u"%1|%2|%3|%4"_s.arg(m_artist.toLower(), m_title.toLower(), m_album.toLower())
                                 .arg(m_length / 1000000);
    const QString key =
        QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/lyrics"_s;
    return u"%1/%2.lrc"_s.arg(dir, key);
}

void LyricsService::lookup(bool bypassCache)
{
    if (!enabled() || m_title.isEmpty()) {
        return;
    }

    if (const QString local = localCandidate(); !local.isEmpty()) {
        QFile file(local);
        if (file.open(QIODevice::ReadOnly)) {
            applyLrc(QString::fromUtf8(file.readAll()), u"file"_s);
            return;
        }
    }

    const QString cache = cachePath();
    if (!bypassCache && !cache.isEmpty() && QFile::exists(cache)) {
        QFile file(cache);
        if (file.open(QIODevice::ReadOnly)) {
            applyLrc(QString::fromUtf8(file.readAll()), u"cache"_s);
            if (!m_lines.isEmpty() || !m_plain.isEmpty()) {
                return;
            }
        }
    }

    if (!bypassCache && m_misses.contains(missKey())) {
        publishState(u"missing"_s);
        return;
    }

    requestRemote(false);
}

/**
 * `get` needs the duration to line up within a couple of seconds, which is
 * exactly what a stream cannot promise; `search` has no such condition, so a
 * miss on the first is worth one more question rather than a shrug.
 */
void LyricsService::requestRemote(bool search)
{
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }

    QUrlQuery query;
    query.addQueryItem(u"track_name"_s, sanitise(m_title));
    if (!m_artist.isEmpty()) {
        query.addQueryItem(u"artist_name"_s, sanitise(m_artist));
    }
    if (!search) {
        if (!m_album.isEmpty()) {
            query.addQueryItem(u"album_name"_s, sanitise(m_album));
        }
        if (m_length > 0) {
            query.addQueryItem(u"duration"_s, QString::number(m_length / 1000000));
        }
    }

    QUrl url(QString(Endpoint) + (search ? u"search"_s : u"get"_s));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      u"atoll/%1 (https://github.com/atoll-shell/atoll)"_s.arg(QStringLiteral(ATOLL_VERSION)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(8000);

    const QString requestedTitle = m_title;
    const QString requestedArtist = m_artist;

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, requestedTitle, requestedArtist, search] {
        QNetworkReply *reply = m_reply;
        if (!reply) {
            return;
        }
        reply->deleteLater();
        m_reply = nullptr;

        if (reply->error() == QNetworkReply::OperationCanceledError) {
            return;
        }
        if (requestedTitle != m_title || requestedArtist != m_artist) {
            return; // The track moved on while we were asking.
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status != 404) {
            publishState(u"error"_s);
            return;
        }

        handleReply(QString::fromUtf8(reply->readAll()), search);
    });
}

void LyricsService::handleReply(const QString &payload, bool wasSearch)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8());

    QJsonObject best;
    if (wasSearch) {
        // Pick the closest duration among the candidates that carry timings.
        const qint64 wanted = m_length / 1000000;
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        const QJsonArray candidates = document.array();
        for (const QJsonValue &value : candidates) {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(u"syncedLyrics"_s).toString().isEmpty()
                && candidate.value(u"plainLyrics"_s).toString().isEmpty()) {
                continue;
            }
            const qint64 duration = qint64(candidate.value(u"duration"_s).toDouble());
            const qint64 distance = wanted > 0 ? qAbs(duration - wanted) : 0;
            const bool better = distance < bestDistance
                || (distance == bestDistance && !candidate.value(u"syncedLyrics"_s).toString().isEmpty()
                    && best.value(u"syncedLyrics"_s).toString().isEmpty());
            if (better) {
                bestDistance = distance;
                best = candidate;
            }
        }
    } else {
        best = document.object();
    }

    const QString synced = best.value(u"syncedLyrics"_s).toString();
    const QString plain = best.value(u"plainLyrics"_s).toString();

    if (!synced.isEmpty()) {
        applyLrc(synced, u"lrclib"_s);
        if (const QString path = cachePath(); !path.isEmpty()) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                file.write(synced.toUtf8());
            }
        }
        return;
    }
    if (!plain.isEmpty()) {
        applyPlain(plain, u"lrclib"_s);
        return;
    }

    if (!wasSearch) {
        requestRemote(true);
        return;
    }

    m_misses.insert(missKey());
    publishState(u"missing"_s);
}

QVariantList LyricsService::parseLrc(const QString &lrc)
{
    static const QRegularExpression stamp(uR"(\[(\d+):(\d+)(?:[.:](\d+))?\])"_s);

    QVariantList lines;
    const QStringList raw = lrc.split(u'\n');
    for (const QString &line : raw) {
        auto it = stamp.globalMatch(line);
        QList<qint64> times;
        int lastEnd = 0;
        while (it.hasNext()) {
            const auto match = it.next();
            if (match.capturedStart() != lastEnd) {
                break; // Timestamps only count while they are still a prefix.
            }
            lastEnd = match.capturedEnd();
            const qint64 minutes = match.captured(1).toLongLong();
            const qint64 seconds = match.captured(2).toLongLong();
            const QString fraction = match.captured(3);
            qint64 millis = 0;
            if (!fraction.isEmpty()) {
                millis = fraction.leftJustified(3, u'0').left(3).toLongLong();
            }
            times.append((minutes * 60 + seconds) * 1000 + millis);
        }
        if (times.isEmpty()) {
            continue;
        }
        const QString text = line.mid(lastEnd).trimmed();
        for (const qint64 time : std::as_const(times)) {
            lines.append(QVariantMap{{u"time"_s, time}, {u"text"_s, text}});
        }
    }

    std::sort(lines.begin(), lines.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(u"time"_s).toLongLong() < b.toMap().value(u"time"_s).toLongLong();
    });
    return lines;
}

void LyricsService::applyLrc(const QString &lrc, const QString &origin)
{
    const QVariantList parsed = parseLrc(lrc);
    if (parsed.isEmpty()) {
        // A file with no timestamps is still lyrics, just not synced ones.
        applyPlain(lrc, origin);
        return;
    }

    m_lines = parsed;
    m_plain.clear();
    m_origin = origin;
    m_trackLabel = m_artist.isEmpty() ? m_title : u"%1 - %2"_s.arg(m_artist, m_title);
    m_currentIndex = -1;
    Q_EMIT linesChanged();
    publishState(u"synced"_s);
    updateCurrentIndex();
}

void LyricsService::applyPlain(const QString &text, const QString &origin)
{
    m_lines.clear();
    m_plain = text.trimmed();
    m_origin = origin;
    m_trackLabel = m_artist.isEmpty() ? m_title : u"%1 - %2"_s.arg(m_artist, m_title);
    if (m_currentIndex != -1) {
        m_currentIndex = -1;
        Q_EMIT currentIndexChanged();
    }
    Q_EMIT linesChanged();
    publishState(m_plain.isEmpty() ? u"missing"_s : u"plain"_s);
}

void LyricsService::updateCurrentIndex()
{
    if (m_lines.isEmpty() || !m_player) {
        return;
    }

    const qint64 offset = m_config->value(u"lyrics.offsetMs"_s, 0).toInt();
    const qint64 now = m_player->position() / 1000 + offset;

    int index = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines.at(i).toMap().value(u"time"_s).toLongLong() <= now) {
            index = i;
        } else {
            break;
        }
    }

    if (index != m_currentIndex) {
        m_currentIndex = index;
        Q_EMIT currentIndexChanged();
    }
}
