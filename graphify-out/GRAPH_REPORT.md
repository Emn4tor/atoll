# Graph Report - atoll  (2026-08-24)

## Corpus Check
- 75 files · ~55,545 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1547 nodes · 2951 edges · 66 communities (64 shown, 2 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 340 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `b2b99779`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- ShellWindow
- LyricsService
- AiService
- ClaudeCliProvider
- AiToolbox
- MprisPlayer
- IpcService
- ShareSender
- ShareService
- Visualizer
- Application
- DBusMessageInfo
- Config
- MprisManager
- aiservice.cpp
- Battery
- ShareServer
- AnthropicProvider
- ShareDiscovery
- ImageStore
- NotificationModel
- shareserver.cpp
- AiProvider
- OsdMonitor
- shareservice.cpp
- mprisplayer.cpp
- NotificationData
- application.cpp
- Atoll
- credentialstore.cpp
- ScreenCapture
- setState
- CommandRunner
- Clock
- NotificationMonitor
- SharePeer
- geminiprovider.cpp
- AiToolCall
- PermissionBroker
- LockMonitor
- ShareIdentity
- main.cpp
- extractImage
- AiBackend
- AiTurn
- QObject
- osdmonitor.cpp
- GeminiProvider
- permissionbroker.cpp
- resolveArt
- notificationmodel.cpp
- shareserver.h
- ShareFile
- QTimer
- QString
- aiservice.h
- QVariantMap
- AiRequest
- install.sh
- AiToolResult
- sharecredentials.cpp
- atollctl
- Packaging
- ConditionalRead
- PermissionBroker::PermissionBroker
- devices

## God Nodes (most connected - your core abstractions)
1. `AiService` - 129 edges
2. `MprisPlayer` - 86 edges
3. `ShareService` - 81 edges
4. `Application` - 70 edges
5. `LyricsService` - 66 edges
6. `ShareServer` - 47 edges
7. `ClaudeCliProvider` - 44 edges
8. `ShellWindow` - 44 edges
9. `IpcService` - 44 edges
10. `ShareSender` - 44 edges

## Surprising Connections (you probably didn't know these)
- `reviewToolCall` --references--> `AiRequest`  [INFERRED]
  src/ai/aiservice.h → src/ai/aiprovider.h
- `testKey` --references--> `AiBackend`  [INFERRED]
  src/ai/aiservice.h → src/ai/aiprovider.h
- `AiService::AiService()` --calls--> `cliDetail`  [INFERRED]
  src/ai/aiservice.cpp → src/ai/aiservice.h
- `classifyCommand` --calls--> `maxRisk()`  [INFERRED]
  src/ai/permissionbroker.h → src/ai/aitypes.h
- `main()` --calls--> `start`  [INFERRED]
  src/main.cpp → src/app/application.h

## Import Cycles
- None detected.

## Communities (66 total, 2 thin omitted)

### Community 0 - "ShellWindow"
Cohesion: 0.06
Nodes (68): Layer, Config, Config, QObject, QQuickWindow, QSize, QString, QVariantList (+60 more)

### Community 1 - "LyricsService"
Cohesion: 0.06
Nodes (62): Config, Config, MprisManager, qint64, QObject, QString, QVariantList, Q_OBJECT (+54 more)

### Community 2 - "AiService"
Cohesion: 0.04
Nodes (51): PendingReview, AiService, activityChanged, answerChanged, cliChanged, configurationChanged, conversationChanged, focusRequested (+43 more)

### Community 3 - "ClaudeCliProvider"
Cohesion: 0.06
Nodes (58): candidatePaths(), ClaudeCliProvider, abort, arguments, busy, ClaudeCliProvider::ClaudeCliProvider(), consume, describe (+50 more)

### Community 4 - "AiToolbox"
Cohesion: 0.09
Nodes (48): QJsonArray, AiToolbox, AiToolbox::AiToolbox(), cancel, clientAddendum, completed, definitions, execute (+40 more)

### Community 5 - "MprisPlayer"
Cohesion: 0.04
Nodes (38): Q_OBJECT, qint64, QObject, QTimer, MprisPlayer, artChanged, capabilitiesChanged, identityChanged (+30 more)

### Community 6 - "IpcService"
Cohesion: 0.07
Nodes (42): OpenReview, QDBusContext, QObject, QString, QStringList, QHash, QObject, QString (+34 more)

### Community 7 - "ShareSender"
Cohesion: 0.07
Nodes (42): QNetworkReply, QList, QNetworkRequest, QObject, QString, ShareCredentials, Q_OBJECT, QHash (+34 more)

### Community 8 - "ShareService"
Cohesion: 0.05
Nodes (38): Q_OBJECT, QDateTime, qint64, QList, QObject, QString, QTimer, quint16 (+30 more)

### Community 9 - "Visualizer"
Cohesion: 0.07
Nodes (36): Config, Config, QList, QObject, Q_OBJECT, QByteArray, QList, QObject (+28 more)

### Community 10 - "Application"
Cohesion: 0.07
Nodes (30): Application, busTapChanged, create, m_ai, m_battery, m_busError, m_busTapActive, m_clock (+22 more)

### Community 11 - "DBusMessageInfo"
Cohesion: 0.07
Nodes (31): DBusMonitor, Kind, QObject, QStringList, DBusMessageInfo, arguments, destination, interface (+23 more)

### Community 12 - "Config"
Cohesion: 0.12
Nodes (30): QFileSystemWatcher, Config, applyWatch, changed, Config::Config(), deepMerge, defaults, defaultValue (+22 more)

### Community 13 - "MprisManager"
Cohesion: 0.11
Nodes (30): MprisPlayer, Config, Config, QObject, QString, QVariantList, Q_OBJECT, QList (+22 more)

### Community 14 - "aiservice.cpp"
Cohesion: 0.13
Nodes (31): send, activeBackend, ask, bringToFront, busy, cliDetail, cliInstallCommand, cliState (+23 more)

### Community 15 - "Battery"
Cohesion: 0.09
Nodes (25): Battery, apply, Battery::Battery(), changed, iconName, m_iconName, m_percent, m_present (+17 more)

### Community 16 - "ShareServer"
Cohesion: 0.07
Nodes (31): qintptr, quint16, ShareCredentials, Connection, Q_OBJECT, QHash, QPointer, QString (+23 more)

### Community 17 - "AnthropicProvider"
Cohesion: 0.11
Nodes (27): AnthropicProvider, AnthropicProvider::AnthropicProvider(), buildBody, buildRequest, handleEvent, m_blocks, m_calls, m_raw (+19 more)

### Community 18 - "ShareDiscovery"
Cohesion: 0.10
Nodes (29): QByteArray, QHostAddress, QList, QObject, quint16, Q_OBJECT, QObject, QSet (+21 more)

### Community 19 - "ImageStore"
Cohesion: 0.12
Nodes (23): QMutex, QPixmap, QQuickImageProvider, QColor, QImage, QSize, QString, QHash (+15 more)

### Community 20 - "NotificationModel"
Cohesion: 0.09
Nodes (24): QAbstractListModel, QModelIndex, Config, QByteArray, QHash, QVariant, QVariantMap, Q_OBJECT (+16 more)

### Community 21 - "shareserver.cpp"
Cohesion: 0.17
Nodes (26): Session, Connection, QByteArray, QObject, QString, reason(), sanitise(), abortSession (+18 more)

### Community 22 - "AiProvider"
Cohesion: 0.11
Nodes (23): AiProvider, abort, AiProvider::AiProvider(), buildBody, buildRequest, busy, consume, describeHttpError (+15 more)

### Community 23 - "OsdMonitor"
Cohesion: 0.10
Nodes (16): Config, DBusMessageInfo, Q_OBJECT, QObject, QString, OsdMonitor, dismissed, m_config (+8 more)

### Community 24 - "shareservice.cpp"
Cohesion: 0.18
Nodes (22): qreal, Config, QObject, QString, defaultAlias(), configure, dataDirectory, destination (+14 more)

### Community 25 - "mprisplayer.cpp"
Cohesion: 0.17
Nodes (21): qint64, QObject, QString, QVariantList, call, fetchAll, iconName, MprisPlayer::MprisPlayer() (+13 more)

### Community 26 - "NotificationData"
Cohesion: 0.09
Nodes (22): QColor, QDateTime, QString, QStringList, NotificationData, accent, actions, appIcon (+14 more)

### Community 27 - "application.cpp"
Cohesion: 0.14
Nodes (20): QUrl, activateApp, adjustVolume, Application::Application(), copyText, debugState, debugSurface, instance (+12 more)

### Community 28 - "Atoll"
Cohesion: 0.10
Nodes (20): Arch, in one go, Atoll, By hand, Configuring, Connecting it, Controlling it, How it gets its information, Installing (+12 more)

### Community 29 - "credentialstore.cpp"
Cohesion: 0.26
Nodes (19): QObject, QString, CredentialStore, backendFor, CredentialStore::CredentialStore(), environmentVariable, filePath, fromFile (+11 more)

### Community 30 - "ScreenCapture"
Cohesion: 0.16
Nodes (20): QObject, QString, QVariantMap, uint, Q_OBJECT, QObject, portalService(), ScreenCapture (+12 more)

### Community 31 - "setState"
Cohesion: 0.21
Nodes (20): addStep, advanceQueue, AiService::AiService(), allow, answerReview, cancel, deny, executeNow (+12 more)

### Community 32 - "CommandRunner"
Cohesion: 0.15
Nodes (17): Job, Q_SIGNALS, CommandRunner, cancelAll, collect, CommandRunner::CommandRunner(), condense, finished (+9 more)

### Community 33 - "Clock"
Cohesion: 0.13
Nodes (16): Clock, Clock::Clock(), date, m_config, m_timer, QML_ELEMENT, scheduleNext, tick (+8 more)

### Community 34 - "NotificationMonitor"
Cohesion: 0.11
Nodes (18): PendingCall, Config, QStringList, Q_OBJECT, QHash, QObject, quint32, quint64 (+10 more)

### Community 35 - "SharePeer"
Cohesion: 0.14
Nodes (15): answer, remember, QDateTime, QString, quint16, QVariantMap, SharePeer, address (+7 more)

### Community 36 - "geminiprovider.cpp"
Cohesion: 0.18
Nodes (15): QByteArray, QJsonObject, QJsonValue, QNetworkAccessManager, QNetworkRequest, QObject, QString, buildBody (+7 more)

### Community 37 - "AiToolCall"
Cohesion: 0.16
Nodes (14): aiRiskName(), AiToolCall, id, input, name, AiVerdict, detail, grantKey (+6 more)

### Community 38 - "PermissionBroker"
Cohesion: 0.15
Nodes (15): QSet, Config, AiRisk, Q_OBJECT, QObject, QSet, QString, PermissionBroker (+7 more)

### Community 39 - "LockMonitor"
Cohesion: 0.18
Nodes (12): QML_UNCREATABLE, QObject, Q_OBJECT, QObject, LockMonitor, lockedChanged, LockMonitor::LockMonitor(), m_locked (+4 more)

### Community 40 - "ShareIdentity"
Cohesion: 0.14
Nodes (13): QHostAddress, QUdpSocket, setIdentity, setIdentity, setIdentity, QJsonObject, ShareIdentity, alias (+5 more)

### Community 41 - "main.cpp"
Cohesion: 0.20
Nodes (11): QQmlApplicationEngine, QString, reply(), runPermissionHook(), QQuickWindow, QString, firstWindow(), main() (+3 more)

### Community 42 - "extractImage"
Cohesion: 0.15
Nodes (14): QImage, Config, DBusMessageInfo, QColor, QImage, QObject, QString, quint64 (+6 more)

### Community 43 - "AiBackend"
Cohesion: 0.14
Nodes (12): AiBackend, abort, busy, defaultModel, failed, id, textDelta, thoughtDelta (+4 more)

### Community 44 - "AiTurn"
Cohesion: 0.15
Nodes (11): AiTurn, image, imageMediaType, rawContent, rawProvider, role, text, toolCalls (+3 more)

### Community 45 - "QObject"
Cohesion: 0.25
Nodes (6): QObject, QVariantList, QHash, QQuickWindow, QStringList, QColor

### Community 46 - "osdmonitor.cpp"
Cohesion: 0.23
Nodes (13): start, Config, DBusMessageInfo, QObject, QString, QStringList, emitEvent, handleMessage (+5 more)

### Community 47 - "GeminiProvider"
Cohesion: 0.18
Nodes (11): GeminiProvider, m_callCounter, m_calls, m_finishReason, m_raw, m_text, public, Q_OBJECT (+3 more)

### Community 48 - "permissionbroker.cpp"
Cohesion: 0.37
Nodes (12): argumentsOf(), QString, QStringList, isAdminProgram(), isForbiddenProgram(), isSecretPath(), classify, classifyCommand (+4 more)

### Community 49 - "resolveArt"
Cohesion: 0.19
Nodes (13): QNetworkAccessManager, QStringList, QVariant, QVariantMap, demarshall(), applyMetadata, applyPlayerProperties, applyRootProperties (+5 more)

### Community 50 - "notificationmodel.cpp"
Cohesion: 0.29
Nodes (12): Config, QObject, QString, quint32, quint64, close, dismiss, indexOfUid (+4 more)

### Community 51 - "shareserver.h"
Cohesion: 0.18
Nodes (9): QTcpServer, QList, QPointer, QProcess, ShareCredentials, QFile, QNetworkAccessManager, QNetworkReply (+1 more)

### Community 52 - "ShareFile"
Cohesion: 0.18
Nodes (12): QList, QStringList, collect, offer, offerPaths, qint64, ShareFile, id (+4 more)

### Community 53 - "QTimer"
Cohesion: 0.20
Nodes (8): QTimer, QDateTime, Config, QNetworkAccessManager, ShareDiscovery, ShareSender, ShareServer, Config

### Community 55 - "aiservice.h"
Cohesion: 0.20
Nodes (9): QQueue, AiToolbox, AnthropicProvider, ClaudeCliProvider, Config, CredentialStore, GeminiProvider, PermissionBroker (+1 more)

### Community 56 - "QVariantMap"
Cohesion: 0.25
Nodes (7): DBusMessage, DBusMessageIter, QVariantMap, argumentsOf(), QVariant, QVariantList, fromIter()

### Community 57 - "AiRequest"
Cohesion: 0.22
Nodes (9): AiRequest, baseUrl, effort, history, maxTokens, model, systemPrompt, tools (+1 more)

### Community 58 - "install.sh"
Cohesion: 0.52
Nodes (6): ask(), die(), note(), say(), install.sh script, warn()

### Community 59 - "AiToolResult"
Cohesion: 0.29
Nodes (7): AiToolResult, content, id, image, imageMediaType, isError, QByteArray

### Community 60 - "sharecredentials.cpp"
Cohesion: 0.67
Nodes (5): QString, ShareCredentials, mint(), read(), ShareCredentials::load()

### Community 61 - "atollctl"
Cohesion: 0.83
Nodes (3): atollctl script, call(), usage()

### Community 62 - "Packaging"
Cohesion: 0.50
Nodes (3): Packaging, Publishing to the AUR, The assistant's client

### Community 63 - "ConditionalRead"
Cohesion: 0.67
Nodes (3): ConditionalRead, program, safeVerbs

### Community 64 - "PermissionBroker::PermissionBroker"
Cohesion: 0.67
Nodes (3): Config, QObject, PermissionBroker::PermissionBroker()

## Knowledge Gaps
- **467 isolated node(s):** `role`, `text`, `toolCalls`, `toolResults`, `image` (+462 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `AiService` connect `AiService` to `AiToolCall`, `LockMonitor`, `application.cpp`, `Application`, `AiTurn`, `QObject`, `aiservice.cpp`, `aiservice.h`, `AiToolResult`, `setState`?**
  _High betweenness centrality (0.195) - this node is a cross-community bridge._
- **Why does `ShareService` connect `ShareService` to `devices`, `SharePeer`, `IpcService`, `LockMonitor`, `ShareIdentity`, `Application`, `ShareFile`, `QTimer`, `shareservice.cpp`, `application.cpp`?**
  _High betweenness centrality (0.156) - this node is a cross-community bridge._
- **Why does `QML_UNCREATABLE` connect `LockMonitor` to `ShellWindow`, `LyricsService`, `AiService`, `Clock`, `MprisPlayer`, `IpcService`, `ShareService`, `Visualizer`, `Config`, `MprisManager`, `Battery`, `OsdMonitor`?**
  _High betweenness centrality (0.115) - this node is a cross-community bridge._
- **What connects `role`, `text`, `toolCalls` to the rest of the system?**
  _467 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `ShellWindow` be split into smaller, more focused modules?**
  _Cohesion score 0.05837837837837838 - nodes in this community are weakly interconnected._
- **Should `LyricsService` be split into smaller, more focused modules?**
  _Cohesion score 0.0579476861167002 - nodes in this community are weakly interconnected._
- **Should `AiService` be split into smaller, more focused modules?**
  _Cohesion score 0.03571428571428571 - nodes in this community are weakly interconnected._