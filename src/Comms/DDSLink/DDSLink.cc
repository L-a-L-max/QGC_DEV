#ifdef QGC_ENABLE_DDS

#include "DDSLink.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>

QGC_LOGGING_CATEGORY(DDSLinkLog, "Comms.DDSLink")

DDSLink::DDSLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
    , _dataInjector(&_mappingEngine, &_transformRegistry, this)
{
    qCDebug(DDSLinkLog) << "DDSLink created";

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

    qCInfo(DDSLinkLog) << "Loaded mapping:" << mappingName
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
    qCInfo(DDSLinkLog) << "DDS link connected on domain" << config->domainId();
    emit connected();
    return true;
}

void DDSLink::disconnect()
{
    if (!_connected) {
        return;
    }

    _pollTimer.stop();
    _readers.clear();

    if (_participant > 0) {
        _destroyParticipant(_participant);
        _participant = DDS_ENTITY_NIL;
    }

    _connected = false;
    emit disconnected();
    qCInfo(DDSLinkLog) << "DDS link disconnected";
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

    qCInfo(DDSLinkLog) << "Created DDS participant on domain" << domainId
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
    Q_UNUSED(participant);

    const DDSConfiguration *config = _ddsConfig();
    const QString prefix = config ? config->namespacePrefix() : QString();

    // Register topics for future subscription.
    // Typed readers require IDL-generated type descriptors; until those are
    // generated, we record the topic list so the mapping engine and
    // data-injector pipeline can be validated end-to-end with injected data.
    for (const QString &topicName : topicNames) {
        QString ddsTopicName = prefix.isEmpty()
                                   ? QStringLiteral("rt") + topicName
                                   : prefix + topicName;

        ReaderInfo info;
        info.reader = DDS_ENTITY_NIL;
        _readers.insert(topicName, info);

        qCDebug(DDSLinkLog) << "Registered topic:" << ddsTopicName
                            << "(reader pending type support)";
    }

    qCInfo(DDSLinkLog) << "Registered" << _readers.size() << "of"
                       << topicNames.size() << "topics (type support pending)";
}

QStringList DDSLink::_runDiscovery(dds_entity_t participant)
{
    Q_UNUSED(participant);
    qCDebug(DDSLinkLog) << "Discovery: using mapping-table topics (builtin discovery deferred)";
    return {};
}

QHash<QString, QVariant> DDSLink::_readSample(dds_entity_t reader, const QString &topicName)
{
    Q_UNUSED(reader);
    Q_UNUSED(topicName);
    // Typed reading requires IDL-generated type descriptors.
    // Will be implemented when PX4 IDL types are generated via idlc.
    return {};
}

#endif // QGC_ENABLE_DDS
