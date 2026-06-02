#ifdef QGC_ENABLE_DDS

#include "DDSLink.h"
#include "DDSDataInjector.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"

#include <QtCore/QDebug>

// CycloneDDS C API
// When integrating: #include <dds/dds.h>
// For now we use stub implementations that compile without the DDS library.
// Replace the stub bodies with real CycloneDDS calls during P1 integration.

static constexpr int kPollIntervalMs = 10;  // 100 Hz polling → <10ms latency

DDSLink::DDSLink(DDSConfiguration *config, QObject *parent)
    : LinkConfiguration(config->name(), parent)
    , _config(config)
{
    _mappingEngine = new DDSMappingEngine(this);
    _transformRegistry = new DDSTransformRegistry(this);
    _dataInjector = new DDSDataInjector(_mappingEngine, _transformRegistry, this);

    _pollTimer.setInterval(kPollIntervalMs);
    connect(&_pollTimer, &QTimer::timeout, this, &DDSLink::_onPollTimer);

    // Wire DDS messages to the data injector
    connect(this, &DDSLink::ddsMessageReceived,
            _dataInjector, &DDSDataInjector::onDDSMessage);
}

DDSLink::~DDSLink()
{
    disconnectLink();
}

bool DDSLink::connectLink()
{
    QMutexLocker locker(&_mutex);
    if (_connected) {
        return true;
    }

    // Load vendor mapping table
    const QString mappingFile = _config->vendorMapping().isEmpty()
                                    ? QStringLiteral("_default")
                                    : _config->vendorMapping();
    if (!_mappingEngine->loadMapping(mappingFile)) {
        qWarning() << "[DDSLink] Failed to load mapping:" << mappingFile;
        return false;
    }
    qInfo() << "[DDSLink] Loaded mapping:" << mappingFile
            << "with" << _mappingEngine->topicCount() << "topics";

    // Create DDS participant
    if (!_createParticipant()) {
        qWarning() << "[DDSLink] Failed to create DDS participant on domain"
                    << _config->domainId();
        return false;
    }

    // Discover or use configured topics
    QStringList topics;
    if (_config->autoDiscover()) {
        topics = _runDiscovery();
        if (!topics.isEmpty()) {
            emit topicsDiscovered(topics);
        }
    }

    // If discovery didn't find anything, subscribe to all topics in mapping table
    if (topics.isEmpty()) {
        topics = _mappingEngine->allTopicNames();
    }

    // Apply namespace prefix
    QStringList prefixedTopics;
    const QString &ns = _config->namespacePrefix();
    for (const QString &t : topics) {
        prefixedTopics.append(ns + t);
    }

    _subscribeToTopics(prefixedTopics);

    _connected = true;
    _pollTimer.start();

    qInfo() << "[DDSLink] Connected. Domain:" << _config->domainId()
            << "Topics:" << _readers.size()
            << "Namespace:" << (ns.isEmpty() ? "(none)" : ns);

    emit connectedChanged(true);
    return true;
}

void DDSLink::disconnectLink()
{
    QMutexLocker locker(&_mutex);
    if (!_connected) {
        return;
    }

    _pollTimer.stop();
    _destroyParticipant();
    _readers.clear();
    _connected = false;

    qInfo() << "[DDSLink] Disconnected";
    emit connectedChanged(false);
}

void DDSLink::setVehicle(Vehicle *vehicle)
{
    if (_dataInjector) {
        _dataInjector->setVehicle(vehicle);
    }
}

void DDSLink::_onPollTimer()
{
    QMutexLocker locker(&_mutex);
    if (!_connected) {
        return;
    }

    for (auto it = _readers.constBegin(); it != _readers.constEnd(); ++it) {
        const QString &topicName = it.key();
        QHash<QString, QVariant> fields = _readSample(topicName);
        if (!fields.isEmpty()) {
            const quint64 tsUs = static_cast<quint64>(
                QDateTime::currentMSecsSinceEpoch() * 1000);
            emit ddsMessageReceived(topicName, fields, tsUs);
        }
    }
}

// ---------------------------------------------------------------------------
// CycloneDDS integration stubs
// Replace these with real dds_create_participant / dds_create_reader / dds_take
// calls when linking against libddsc.
// ---------------------------------------------------------------------------

bool DDSLink::_createParticipant()
{
    // TODO(P1): Replace with:
    //   _participantHandle = dds_create_participant(_config->domainId(), NULL, NULL);
    //   return _participantHandle >= 0;
    qInfo() << "[DDSLink] STUB: _createParticipant domain=" << _config->domainId();
    _participantHandle = 1;  // fake handle
    return true;
}

void DDSLink::_destroyParticipant()
{
    // TODO(P1): Replace with:
    //   if (_participantHandle >= 0) dds_delete(_participantHandle);
    _participantHandle = -1;
}

void DDSLink::_subscribeToTopics(const QStringList &topicNames)
{
    // TODO(P1): For each topic, use DDS Dynamic Data API to create a reader
    //   without compile-time IDL types. This allows subscribing to any PX4 topic.
    //
    //   Pseudocode:
    //   for (const auto &name : topicNames) {
    //       dds_entity_t topic = dds_create_topic_generic(_participantHandle, ...);
    //       dds_entity_t reader = dds_create_reader(_participantHandle, topic, ...);
    //       _readers.insert(name, reader);
    //   }
    for (const QString &name : topicNames) {
        _readers.insert(name, 0);  // fake reader handle
    }
    qInfo() << "[DDSLink] STUB: Subscribed to" << topicNames.size() << "topics";
}

QStringList DDSLink::_runDiscovery()
{
    // TODO(P1): Use DDS built-in topic discovery to enumerate available topics.
    //   dds_entity_t reader = dds_create_reader(_participantHandle,
    //       DDS_BUILTIN_TOPIC_DCPSPUBLICATION, ...);
    //   Then read DCPSPublication samples to get topic names/types.
    qInfo() << "[DDSLink] STUB: _runDiscovery";
    return {};
}

QHash<QString, QVariant> DDSLink::_readSample(const QString &topicName)
{
    // TODO(P1): Use DDS Dynamic Data API to read one sample from the reader,
    //   extract fields by name, and return them as QVariant key-value pairs.
    //
    //   Pseudocode:
    //   int reader = _readers.value(topicName);
    //   void *samples[1];
    //   dds_sample_info_t infos[1];
    //   if (dds_take(reader, samples, infos, 1, 1) > 0 && infos[0].valid_data) {
    //       // extract fields using dynamic type introspection
    //       return extractedFields;
    //   }
    Q_UNUSED(topicName);
    return {};
}

#endif // QGC_ENABLE_DDS
