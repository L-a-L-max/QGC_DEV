#ifdef QGC_ENABLE_DDS

#include "DDSLink.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkInterface>

QGC_LOGGING_CATEGORY_ON(DDSLinkLog, "Comms.DDSLink")

// --- Shared participant pool (one participant per DDS domain) ---
// CycloneDDS does not reliably deliver data when multiple participants
// in the same process share a domain — match diagnostics report correct
// values but actual UDP delivery fails for participants created after
// the first.  All DDSLinks (and DDSDiscovery) on the same domain must
// use a single shared participant.
QHash<int, dds_entity_t> DDSLink::s_domainParticipants;
QHash<int, int>          DDSLink::s_domainRefCounts;

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

    qInfo() << "[DDSLink] Loaded mapping:" << mappingName
             << "topics:" << _mappingEngine.topicCount()
             << "fields:" << _mappingEngine.fieldCount();

    // Use the domain-wide shared participant.  All DDSLinks on the same
    // domain share one CycloneDDS participant to avoid the multi-participant
    // data-delivery bug (see s_domainParticipants comment above).
    _domainId = config->domainId();
    _participant = acquireSharedParticipant(_domainId);
    if (_participant <= 0) {
        emit communicationError(tr("DDS Link"), tr("Failed to create DDS participant"));
        return false;
    }
    qInfo() << "[DDSLink] Using shared participant on domain"
            << _domainId << "entity:" << _participant;

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

    _pollTimer.start();

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
        const int hbSubs  = _heartbeatPublisher.matchedSubscriptionCount();
        const int cmdSubs = _commandPublisher.matchedSubscriptionCount();
        qInfo() << "[DDSLink] Writer match diagnostic (5s): heartbeat_writer→"
                << hbSubs << "subscriber(s), command_writer→"
                << cmdSubs << "subscriber(s)";
        if (hbSubs == 0) {
            qWarning() << "[DDSLink] Heartbeat writer has NO matched subscribers!"
                       << "PX4 will report 'Connection to ground station lost'.";
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

    // Deinit publishers before destroying participant
    _heartbeatPublisher.deinit();
    _commandPublisher.deinit();

    // Delete readers explicitly before destroying participant
    for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
        if (it.value().reader > 0) {
            dds_delete(it.value().reader);
        }
    }
    _readers.clear();
    _receivedTopics.clear();

    // Release our reference to the shared domain participant.
    // The participant is only destroyed when the last link on this domain
    // disconnects — other links' readers/writers keep it alive.
    if (_participant > 0 && _domainId >= 0) {
        releaseSharedParticipant(_domainId);
        _participant = DDS_ENTITY_NIL;
    }

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

    for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
        if (it.value().reader <= 0) {
            continue;
        }
        const QHash<QString, QVariant> sample = _readSample(it.value().reader, it.key());
        if (!sample.isEmpty()) {
            if (!_receivedTopics.contains(it.key())) {
                _receivedTopics.insert(it.key());
                qInfo() << "[DDSLink] First data received from" << it.key()
                         << "fields:" << sample.size();
            }
            emit ddsMessageReceived(it.key(), sample, now);
        }
    }
}

// ---------------------------------------------------------------------------
// CycloneDDS implementations
// ---------------------------------------------------------------------------

dds_entity_t DDSLink::_createParticipant(int domainId)
{
    // Only set config if not already configured by user
    if (qEnvironmentVariableIsEmpty("CYCLONEDDS_URI")) {
        // Enumerate all physical network interfaces (skip loopback, docker, bridges)
        // CycloneDDS by default picks ONE interface arbitrarily; if PX4 Agent is on
        // a different subnet, SEDP unicast messages never reach it.
        QString interfacesXml;
        const auto allIfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &iface : allIfaces) {
            if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
            if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
            const QString name = iface.name();
            // Skip virtual/container interfaces
            if (name.startsWith(QStringLiteral("docker")) ||
                name.startsWith(QStringLiteral("br-")) ||
                name.startsWith(QStringLiteral("veth")) ||
                name.startsWith(QStringLiteral("virbr"))) {
                continue;
            }
            // Must have at least one IPv4 address
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

        QString config;
        if (interfacesXml.isEmpty()) {
            // Fallback: let CycloneDDS choose automatically
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

dds_entity_t DDSLink::acquireSharedParticipant(int domainId)
{
    auto it = s_domainParticipants.find(domainId);
    if (it != s_domainParticipants.end() && *it > 0) {
        s_domainRefCounts[domainId]++;
        qInfo() << "[DDSLink] Reusing shared participant on domain" << domainId
                << "entity:" << *it << "refcount:" << s_domainRefCounts[domainId];
        return *it;
    }

    // No shared participant yet — build CycloneDDS config and create one.
    if (qEnvironmentVariableIsEmpty("CYCLONEDDS_URI")) {
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
        }

        QString config;
        if (interfacesXml.isEmpty()) {
            config = QStringLiteral(
                "<CycloneDDS>"
                "  <Domain id=\"any\">"
                "    <Compatibility>"
                "      <StandardsConformance>lax</StandardsConformance>"
                "    </Compatibility>"
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
                "  </Domain>"
                "</CycloneDDS>").arg(interfacesXml);
        }
        qputenv("CYCLONEDDS_URI", config.toUtf8());
    }

    const dds_entity_t p = dds_create_participant(
        static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
    if (p < 0) {
        qWarning() << "[DDSLink] dds_create_participant failed on domain"
                    << domainId << ":" << dds_strretcode(-p);
        return p;
    }

    s_domainParticipants[domainId] = p;
    s_domainRefCounts[domainId] = 1;
    qInfo() << "[DDSLink] Created shared participant on domain" << domainId
            << "entity:" << p;
    return p;
}

void DDSLink::releaseSharedParticipant(int domainId)
{
    auto refIt = s_domainRefCounts.find(domainId);
    if (refIt == s_domainRefCounts.end()) return;

    (*refIt)--;
    qInfo() << "[DDSLink] Released shared participant on domain" << domainId
            << "refcount:" << *refIt;

    if (*refIt <= 0) {
        auto pIt = s_domainParticipants.find(domainId);
        if (pIt != s_domainParticipants.end() && *pIt > 0) {
            dds_delete(*pIt);
            qInfo() << "[DDSLink] Destroyed shared participant on domain" << domainId;
        }
        s_domainParticipants.remove(domainId);
        s_domainRefCounts.remove(domainId);
    }
}

void DDSLink::_subscribeToTopics(dds_entity_t participant, const QStringList &topicNames)
{
    const DDSConfiguration *config = _ddsConfig();
    const QString ns = config ? config->namespacePrefix() : QString();
    const QString rtPrefix = ns.isEmpty() ? QStringLiteral("rt")
                                          : QStringLiteral("rt/") + ns;

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
