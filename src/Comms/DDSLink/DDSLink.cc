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

    if (_participant >= 0) {
        _destroyParticipant(_participant);
        _participant = -1;
    }

    _connected = false;
    emit disconnected();
    qCInfo(DDSLinkLog) << "DDS link disconnected";
}

void DDSLink::_writeBytes(const QByteArray &bytes)
{
    Q_UNUSED(bytes);
    // TODO(P3): Implement DDS publish for command/parameter requests
}

void DDSLink::_onPollTimer()
{
    if (!_connected) {
        return;
    }

    const quint64 now = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000;

    for (auto it = _readers.cbegin(); it != _readers.cend(); ++it) {
        const QHash<QString, QVariant> sample = _readSample(it.value());
        if (!sample.isEmpty()) {
            emit ddsMessageReceived(it.key(), sample, now);
        }
    }
}

// ---------------------------------------------------------------------------
// CycloneDDS stub implementations — replaced with real API in P1
// ---------------------------------------------------------------------------

int DDSLink::_createParticipant(int domainId)
{
    // TODO(P1): dds_create_participant(domainId, nullptr, nullptr)
    qCDebug(DDSLinkLog) << "Stub: create participant on domain" << domainId;
    return 1;
}

void DDSLink::_destroyParticipant(int participant)
{
    // TODO(P1): dds_delete(participant)
    qCDebug(DDSLinkLog) << "Stub: destroy participant" << participant;
}

void DDSLink::_subscribeToTopics(int participant, const QStringList &topicNames)
{
    // TODO(P1): For each topic, create reader via dds_create_reader()
    Q_UNUSED(participant);
    int readerId = 100;
    for (const QString &topic : topicNames) {
        _readers.insert(topic, readerId++);
        qCDebug(DDSLinkLog) << "Stub: subscribed to" << topic << "reader:" << (readerId - 1);
    }
}

QStringList DDSLink::_runDiscovery(int participant)
{
    // TODO(P1): Use DDS builtin topics to discover available topics
    Q_UNUSED(participant);
    qCDebug(DDSLinkLog) << "Stub: discovery returned empty list";
    return {};
}

QHash<QString, QVariant> DDSLink::_readSample(int reader)
{
    // TODO(P1): dds_read() / dds_take() to get actual DDS samples
    Q_UNUSED(reader);
    return {};
}

#endif // QGC_ENABLE_DDS
