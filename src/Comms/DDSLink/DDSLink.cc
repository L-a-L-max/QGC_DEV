#ifdef QGC_ENABLE_DDS

#include "DDSLink.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkInterface>

QGC_LOGGING_CATEGORY_ON(DDSLinkLog, "Comms.DDSLink")

DDSLink::DDSLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
    , _dataInjector(&_mappingEngine, &_transformRegistry, this)
    , _vehicleManager(this, this)
{
    qCInfo(DDSLinkLog) << "DDSLink created";

    _pollTimer.setInterval(10);
    (void) connect(&_pollTimer, &QTimer::timeout, this, &DDSLink::_onPollTimer);
    (void) connect(this, &DDSLink::ddsMessageReceived,
                   &_dataInjector, &DDSDataInjector::onDDSMessage);

    // Watchdog timer: fires every 1s to check if data reception has timed out
    _timeoutTimer.setInterval(1000);
    (void) connect(&_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (!_connected || _lastDataReceivedMs == 0) {
            return;
        }
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - _lastDataReceivedMs;
        if (elapsed > kDataTimeoutMs && !_timeoutNotified) {
            _timeoutNotified = true;
            qWarning() << "[DDSLink] Data reception timeout:" << elapsed
                       << "ms since last data (threshold:" << kDataTimeoutMs << "ms)";
            emit communicationError(tr("DDS Link"),
                tr("No data received for %1 seconds — connection may be lost")
                    .arg(elapsed / 1000));
        }
    });
}

DDSLink::~DDSLink()
{
    disconnect();
}

DDSConfiguration *DDSLink::_ddsConfig() const
{
    return qobject_cast<DDSConfiguration *>(_config.get());
}

bool DDSLink::_connect()
{
    if (_connected) {
        return true;
    }

    const DDSConfiguration *config = _ddsConfig();
    if (!config) {
        emit communicationError(tr("DDS Link"), tr("Invalid DDS configuration"));
        return false;
    }

    const QString mappingName = config->vendorMapping().isEmpty()
                                    ? QStringLiteral("_default")
                                    : config->vendorMapping();
    if (!_mappingEngine.loadMapping(mappingName)) {
        qCWarning(DDSLinkLog) << "Failed to load mapping:" << mappingName;
        emit communicationError(tr("DDS Link"),
                                tr("Failed to load mapping table: %1").arg(mappingName));
        return false;
    }

    // Apply IDL version from mapping (or config override)
    const QString idlVer = config->idlVersion().isEmpty()
                               ? _mappingEngine.idlVersion()
                               : config->idlVersion();
    _typeRegistry.setIdlVersion(idlVer);

    qInfo() << "[DDSLink] Loaded mapping:" << mappingName
             << "topics:" << _mappingEngine.topicCount()
             << "fields:" << _mappingEngine.fieldCount()
             << "idl_version:" << idlVer;

    // When Zenoh bridge is enabled, start the local bridge subprocess first.
    // The bridge connects to the remote Zenoh router and creates a local DDS
    // participant — DDSLink then talks to it over localhost DDS.
    if (config->zenohBridge()) {
        // Ensure previous bridge instance is fully stopped before restarting
        if (_zenohBridge.isRunning()) {
            qInfo() << "[DDSLink] Stopping stale Zenoh bridge before reconnect";
            _zenohBridge.stop();
        }
        if (!_zenohBridge.start(config->zenohEndpoint(), config->domainId())) {
            qWarning() << "[DDSLink] Zenoh bridge failed to start:"
                       << _zenohBridge.lastError();
            emit communicationError(tr("DDS Link"),
                tr("Zenoh bridge failed: %1").arg(_zenohBridge.lastError()));
            return false;
        }
        qInfo() << "[DDSLink] Zenoh bridge started, endpoint:"
                << config->zenohEndpoint();
    }

    _participant = _createParticipant(config->domainId(), config->zenohBridge());
    if (_participant < 0) {
        if (config->zenohBridge()) _zenohBridge.stop();
        emit communicationError(tr("DDS Link"), tr("Failed to create DDS participant"));
        return false;
    }

    if (config->autoDiscover()) {
        const QStringList discovered = _runDiscovery(_participant);
        if (!discovered.isEmpty()) {
            qCInfo(DDSLinkLog) << "Discovered topics:" << discovered;
            emit topicsDiscovered();
        }
    }

    _subscribeToTopics(_participant, _mappingEngine.allTopicNames());

    // Initialize command publisher (DDS writer for sending commands to PX4)
    const QString nsPrefix = config->namespacePrefix().isEmpty()
                                 ? QStringLiteral("rt/")
                                 : QStringLiteral("rt/") + config->namespacePrefix() + QStringLiteral("/");
    if (_commandPublisher.init(_participant, nsPrefix)) {
        qInfo() << "[DDSLink] Command publisher ready";
    } else {
        qWarning() << "[DDSLink] Command publisher init failed (commands will not work)";
    }

    // Start periodic GCS heartbeat so PX4 recognises this as a connected GCS
    if (_heartbeatPublisher.init(_participant, nsPrefix)) {
        qInfo() << "[DDSLink] GCS heartbeat publisher ready";
    } else {
        qWarning() << "[DDSLink] GCS heartbeat init failed (pre-arm GCS check may fail)";
    }

    // Virtual joystick publisher (manual_control_input → PX4)
    if (_manualControlPublisher.init(_participant, nsPrefix)) {
        qInfo() << "[DDSLink] Manual control publisher ready";
    } else {
        qWarning() << "[DDSLink] Manual control publisher init failed (virtual joystick will not work)";
    }

    // GotoSetpoint publisher (goto_setpoint → PX4)
    if (_gotoPublisher.init(_participant, nsPrefix)) {
        qInfo() << "[DDSLink] Goto publisher ready";
    } else {
        qWarning() << "[DDSLink] Goto publisher init failed (goto/mission will not work)";
    }

    // Wire mission manager to publishers
    _missionManager.setGotoPublisher(&_gotoPublisher);
    _missionManager.setCommandPublisher(&_commandPublisher);

    // Wire Skydroid joystick to manual control publisher
    _skydroidJoystick.setManualControlPublisher(&_manualControlPublisher);
    _skydroidJoystick.setEnabled(config->skydroidJoystick());

    _pollTimer.start();
    _lastDataReceivedMs = QDateTime::currentMSecsSinceEpoch();
    _timeoutNotified = false;
    _timeoutTimer.start();

    _connected = true;
    qInfo() << "[DDSLink] DDS link connected on domain" << config->domainId();
    emit connected();

    // Early diagnostic at 3s: probe reader-writer matching status
    QTimer::singleShot(3000, this, [this]() {
        if (!_connected || _participant <= 0) return;
        int totalMatched = 0;
        int probed = 0;
        for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
            if (it.value().reader <= 0) continue;
            dds_instance_handle_t handles[10];
            const int n = dds_get_matched_publications(it.value().reader, handles, 10);
            if (n > 0) ++totalMatched;
            if (probed < 3) {
                qInfo() << "[DDSLink] Early probe:" << it.key() << "→" << n << "matched writer(s)";
            }
            ++probed;
        }
        qInfo() << "[DDSLink] Discovery check (3s):" << totalMatched
                 << "of" << probed << "readers have matched a writer";
    });

    // Delayed diagnostic: check RTPS writer matching after discovery period
    QTimer::singleShot(5000, this, [this]() {
        if (!_connected) return;
        int matchedCount = 0;
        int unmatchedCount = 0;
        for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
            if (it.value().reader <= 0) continue;
            dds_instance_handle_t handles[10];
            const int n = dds_get_matched_publications(it.value().reader, handles, 10);
            if (n > 0) {
                matchedCount++;
            } else {
                unmatchedCount++;
            }
        }
        qInfo() << "[DDSLink] RTPS match diagnostic (5s):" << matchedCount
                 << "readers matched a writer," << unmatchedCount << "unmatched";
        if (unmatchedCount > 0 && matchedCount == 0) {
            qWarning() << "[DDSLink] No readers matched any writer!"
                        << "Possible causes: DDS type name mismatch, network issue,"
                        << "or PX4 DDS Agent not publishing."
                        << "Run 'ros2 topic info /fmu/out/vehicle_status_v1 --verbose'"
                        << "to check publisher type name.";
        }
    });

    // Second diagnostic at 15s with per-reader detail
    QTimer::singleShot(15000, this, [this]() {
        if (!_connected) return;
        for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
            if (it.value().reader <= 0) continue;
            dds_instance_handle_t handles[10];
            const int n = dds_get_matched_publications(it.value().reader, handles, 10);
            if (n > 0) {
                qInfo() << "[DDSLink] MATCHED:" << it.key() << "→" << n << "writer(s)";
            } else {
                qInfo() << "[DDSLink] UNMATCHED:" << it.key();
            }
        }
    });

    return true;
}

void DDSLink::disconnect()
{
    if (!_connected) {
        return;
    }

    _pollTimer.stop();
    _timeoutTimer.stop();

    // Deinit publishers before destroying participant
    _heartbeatPublisher.deinit();
    _commandPublisher.deinit();
    _manualControlPublisher.deinit();
    _gotoPublisher.deinit();

    // Delete readers explicitly before destroying participant
    for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
        if (it.value().reader > 0) {
            dds_delete(it.value().reader);
        }
    }
    _readers.clear();
    _receivedTopics.clear();

    if (_participant > 0) {
        _destroyParticipant(_participant);
        _participant = DDS_ENTITY_NIL;
    }

    _zenohBridge.stop();

    _connected = false;
    emit disconnected();
    qInfo() << "[DDSLink] DDS link disconnected";
}

void DDSLink::_writeBytes(const QByteArray &bytes)
{
    Q_UNUSED(bytes);
}

void DDSLink::_onPollTimer()
{
    if (!_connected) {
        return;
    }

    const quint64 now = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000;
    bool gotData = false;

    for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
        if (it.value().reader <= 0) {
            continue;
        }
        const QHash<QString, QVariant> sample = _readSample(it.value().reader, it.key());
        if (!sample.isEmpty()) {
            gotData = true;
            if (!_receivedTopics.contains(it.key())) {
                _receivedTopics.insert(it.key());
                qInfo() << "[DDSLink] First data received from" << it.key()
                         << "fields:" << sample.size();
            }
            emit ddsMessageReceived(it.key(), sample, now);
        }
    }

    if (gotData) {
        _lastDataReceivedMs = QDateTime::currentMSecsSinceEpoch();
        if (_timeoutNotified) {
            _timeoutNotified = false;
            qInfo() << "[DDSLink] Data reception resumed";
        }
    }
}

// ---------------------------------------------------------------------------
// CycloneDDS implementations
// ---------------------------------------------------------------------------

dds_entity_t DDSLink::_createParticipant(int domainId, bool localhostOnly)
{
    // Only set config if not already configured by user
    if (qEnvironmentVariableIsEmpty("CYCLONEDDS_URI")) {
        QString config;

        if (localhostOnly) {
            // Zenoh bridge mode: restrict DDS discovery to localhost only.
            // The bridge subprocess handles remote Zenoh communication;
            // DDSLink only needs to talk to the local bridge over loopback.
            config = QStringLiteral(
                "<CycloneDDS>"
                "  <Domain id=\"any\">"
                "    <General>"
                "      <Interfaces>"
                "        <NetworkInterface address=\"127.0.0.1\" multicast=\"true\"/>"
                "      </Interfaces>"
                "      <AllowMulticast>spdp</AllowMulticast>"
                "    </General>"
                "    <Compatibility>"
                "      <StandardsConformance>lax</StandardsConformance>"
                "    </Compatibility>"
                "    <Tracing>"
                "      <Category>discovery</Category>"
                "      <OutputFile>stderr</OutputFile>"
                "      <Verbosity>config</Verbosity>"
                "    </Tracing>"
                "  </Domain>"
                "</CycloneDDS>");
            qInfo() << "[DDSLink] CycloneDDS configured for localhost (Zenoh bridge mode)";
        } else {
            // Direct DDS mode: enumerate all physical network interfaces
            QString interfacesXml;
            const auto allIfaces = QNetworkInterface::allInterfaces();
            for (const QNetworkInterface &iface : allIfaces) {
                if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
                if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
                const QString name = iface.name();
                if (name.startsWith(QStringLiteral("docker")) ||
                    name.startsWith(QStringLiteral("br-")) ||
                    name.startsWith(QStringLiteral("veth")) ||
                    name.startsWith(QStringLiteral("virbr"))) {
                    continue;
                }
                bool hasIpv4 = false;
                for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        hasIpv4 = true;
                        break;
                    }
                }
                if (!hasIpv4) continue;
                interfacesXml += QStringLiteral("        <NetworkInterface name=\"%1\" multicast=\"true\"/>\n").arg(name);
                qInfo() << "[DDSLink] Using network interface:" << name;
            }

            if (interfacesXml.isEmpty()) {
                config = QStringLiteral(
                    "<CycloneDDS>"
                    "  <Domain id=\"any\">"
                    "    <Compatibility>"
                    "      <StandardsConformance>lax</StandardsConformance>"
                    "    </Compatibility>"
                    "    <Tracing>"
                    "      <Category>discovery</Category>"
                    "      <OutputFile>stderr</OutputFile>"
                    "      <Verbosity>config</Verbosity>"
                    "    </Tracing>"
                    "  </Domain>"
                    "</CycloneDDS>");
            } else {
                config = QStringLiteral(
                    "<CycloneDDS>"
                    "  <Domain id=\"any\">"
                    "    <General>"
                    "      <Interfaces>\n%1"
                    "      </Interfaces>"
                    "    </General>"
                    "    <Compatibility>"
                    "      <StandardsConformance>lax</StandardsConformance>"
                    "    </Compatibility>"
                    "    <Tracing>"
                    "      <Category>discovery</Category>"
                    "      <OutputFile>stderr</OutputFile>"
                    "      <Verbosity>config</Verbosity>"
                    "    </Tracing>"
                    "  </Domain>"
                    "</CycloneDDS>").arg(interfacesXml);
            }
        }

        qputenv("CYCLONEDDS_URI", config.toUtf8());
    }

    const dds_entity_t participant = dds_create_participant(
        static_cast<dds_domainid_t>(domainId), nullptr, nullptr);

    if (participant < 0) {
        qCWarning(DDSLinkLog) << "dds_create_participant failed:" << dds_strretcode(-participant);
        return participant;
    }

    qInfo() << "[DDSLink] Created DDS participant on domain" << domainId
             << "entity:" << participant;
    return participant;
}

void DDSLink::_destroyParticipant(dds_entity_t participant)
{
    const dds_return_t rc = dds_delete(participant);
    if (rc != DDS_RETCODE_OK) {
        qCWarning(DDSLinkLog) << "dds_delete(participant) failed:" << dds_strretcode(-rc);
    } else {
        qCDebug(DDSLinkLog) << "Destroyed DDS participant" << participant;
    }
}

void DDSLink::_subscribeToTopics(dds_entity_t participant, const QStringList &topicNames)
{
    const DDSConfiguration *config = _ddsConfig();
    const QString prefix = config ? config->namespacePrefix() : QString();
    const QString rtPrefix = prefix.isEmpty() ? QStringLiteral("rt") : prefix;

    int typedCount = 0;
    int stubCount = 0;

    for (const QString &topicName : topicNames) {
        const DDSTopicMapping *mapping = _mappingEngine.topicMapping(topicName);
        const QString typeName = mapping ? mapping->ddsTypeName : QString();
        const DDSTypeEntry *typeEntry = typeName.isEmpty() ? nullptr
                                                           : _typeRegistry.typeEntry(typeName);

        if (!typeEntry || !typeEntry->descriptor) {
            ReaderInfo info;
            info.reader = DDS_ENTITY_NIL;
            _readers.insert(topicName, info);
            stubCount++;
            qCDebug(DDSLinkLog) << "No IDL type for" << topicName << "(stub)";
            continue;
        }

        // The topic name from the config IS the actual DDS network topic name.
        // Users configure the exact PX4 topic name in the mapping JSON
        // (e.g. "/fmu/out/vehicle_status_v1" for PX4 v1.17).
        const QString ddsTopicName = rtPrefix + topicName;

        const dds_entity_t topic = dds_create_topic(
            participant, typeEntry->descriptor,
            ddsTopicName.toUtf8().constData(), nullptr, nullptr);

        if (topic < 0) {
            qCWarning(DDSLinkLog) << "dds_create_topic failed for" << ddsTopicName
                                  << ":" << dds_strretcode(-topic);
            ReaderInfo info;
            info.reader = DDS_ENTITY_NIL;
            _readers.insert(topicName, info);
            stubCount++;
            continue;
        }

        // PX4 MicroXRCE-DDS Agent publishes with BEST_EFFORT reliability and
        // TRANSIENT_LOCAL durability. Match both to ensure QoS compatibility.
        dds_qos_t *qos = dds_create_qos();
        dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
        dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
        dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

        const dds_entity_t reader = dds_create_reader(participant, topic, qos, nullptr);
        dds_delete_qos(qos);

        if (reader < 0) {
            qCWarning(DDSLinkLog) << "dds_create_reader failed for" << ddsTopicName
                                  << ":" << dds_strretcode(-reader);
            ReaderInfo info;
            info.reader = DDS_ENTITY_NIL;
            _readers.insert(topicName, info);
            stubCount++;
            continue;
        }

        ReaderInfo info;
        info.reader = reader;
        info.extractor = typeEntry->extractor;
        _readers.insert(topicName, info);
        typedCount++;
        qInfo() << "[DDSLink] Created typed reader for" << ddsTopicName
                 << "type:" << typeName;
    }

    qInfo() << "[DDSLink] Subscribed:" << typedCount << "typed readers,"
             << stubCount << "stubs, of" << topicNames.size() << "topics";
}

QStringList DDSLink::_runDiscovery(dds_entity_t participant)
{
    Q_UNUSED(participant);
    qCDebug(DDSLinkLog) << "Discovery: using mapping-table topics (builtin discovery deferred)";
    return {};
}

QHash<QString, QVariant> DDSLink::_readSample(dds_entity_t reader, const QString &topicName)
{
    // Find the extractor for this topic
    auto it = _readers.constFind(topicName);
    if (it == _readers.constEnd() || !it.value().extractor) {
        return {};
    }

    void *samples[1] = {nullptr};
    dds_sample_info_t infos[1];

    const dds_return_t rc = dds_take(reader, samples, infos, 1, 1);
    if (rc <= 0 || !infos[0].valid_data || !samples[0]) {
        if (samples[0]) {
            // Return the loan
            dds_return_loan(reader, samples, rc);
        }
        return {};
    }

    // Extract fields using the type-specific extractor
    QHash<QString, QVariant> result = it.value().extractor(samples[0]);

    dds_return_loan(reader, samples, rc);
    return result;
}

#endif // QGC_ENABLE_DDS
