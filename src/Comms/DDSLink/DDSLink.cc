#ifdef QGC_ENABLE_DDS

#include "DDSLink.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>

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

    _participant = _createParticipant(config->domainId());
    if (_participant < 0) {
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
    _pollTimer.start();

    _connected = true;
    qInfo() << "[DDSLink] DDS link connected on domain" << config->domainId();
    emit connected();
    return true;
}

void DDSLink::disconnect()
{
    if (!_connected) {
        return;
    }

    _pollTimer.stop();

    // Delete readers explicitly before destroying participant
    for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
        if (it.value().reader > 0) {
            dds_delete(it.value().reader);
        }
    }
    _readers.clear();

    if (_participant > 0) {
        _destroyParticipant(_participant);
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
            continue;  // skip topics without typed readers
        }
        const QHash<QString, QVariant> sample = _readSample(it.value().reader, it.key());
        if (!sample.isEmpty()) {
            emit ddsMessageReceived(it.key(), sample, now);
        }
    }
}

// ---------------------------------------------------------------------------
// CycloneDDS implementations
// ---------------------------------------------------------------------------

dds_entity_t DDSLink::_createParticipant(int domainId)
{
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

    int typedCount = 0;
    int stubCount = 0;

    for (const QString &topicName : topicNames) {
        // Build full DDS topic name: "rt" prefix + topic path
        const QString ddsTopicName = prefix.isEmpty()
                                       ? QStringLiteral("rt") + topicName
                                       : prefix + topicName;

        // Look up the type from the mapping table
        const DDSTopicMapping *mapping = _mappingEngine.topicMapping(topicName);
        const QString typeName = mapping ? mapping->ddsTypeName : QString();
        const DDSTypeEntry *typeEntry = typeName.isEmpty() ? nullptr
                                                           : _typeRegistry.typeEntry(typeName);

        ReaderInfo info;

        if (typeEntry && typeEntry->descriptor) {
            // Create a real DDS topic + reader with the IDL-generated type
            const dds_entity_t topic = dds_create_topic(
                participant, typeEntry->descriptor,
                ddsTopicName.toUtf8().constData(), nullptr, nullptr);

            if (topic < 0) {
                qCWarning(DDSLinkLog) << "dds_create_topic failed for" << ddsTopicName
                                      << ":" << dds_strretcode(-topic);
                info.reader = DDS_ENTITY_NIL;
            } else {
                const dds_entity_t reader = dds_create_reader(
                    participant, topic, nullptr, nullptr);

                if (reader < 0) {
                    qCWarning(DDSLinkLog) << "dds_create_reader failed for" << ddsTopicName
                                          << ":" << dds_strretcode(-reader);
                    info.reader = DDS_ENTITY_NIL;
                } else {
                    info.reader = reader;
                    info.extractor = typeEntry->extractor;
                    typedCount++;
                    qCDebug(DDSLinkLog) << "Created typed reader for" << ddsTopicName;
                }
            }
        } else {
            // No IDL type available — register as stub for future support
            info.reader = DDS_ENTITY_NIL;
            stubCount++;
            qCDebug(DDSLinkLog) << "No IDL type for" << ddsTopicName << "(stub)";
        }

        _readers.insert(topicName, info);
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
