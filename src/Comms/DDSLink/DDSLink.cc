#ifdef QGC_ENABLE_DDS

#include "DDSLink.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QDateTime>

extern "C" {
#include "VehicleAttitude.h"
#include "VehicleGlobalPosition.h"
#include "VehicleLocalPosition.h"
#include "BatteryStatus.h"
#include "VehicleStatus.h"
}

QGC_LOGGING_CATEGORY(DDSLinkLog, "Comms.DDSLink")

// Mapping from our JSON topic names to PX4 XRCE-DDS type names
static const QHash<QString, QString> kTopicTypeMap = {
    {QStringLiteral("/fmu/out/vehicle_attitude"),        QStringLiteral("vehicle_attitude")},
    {QStringLiteral("/fmu/out/vehicle_global_position"), QStringLiteral("vehicle_global_position")},
    {QStringLiteral("/fmu/out/vehicle_local_position"),  QStringLiteral("vehicle_local_position")},
    {QStringLiteral("/fmu/out/battery_status"),           QStringLiteral("battery_status")},
    {QStringLiteral("/fmu/out/vehicle_status"),           QStringLiteral("vehicle_status")},
};

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
// CycloneDDS real implementations (P1)
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
    const DDSConfiguration *config = _ddsConfig();
    const QString prefix = config ? config->namespacePrefix() : QString();

    for (const QString &topicName : topicNames) {
        const dds_topic_descriptor_t *desc = _descriptorForTopic(topicName);
        if (!desc) {
            qCDebug(DDSLinkLog) << "No type descriptor for topic:" << topicName << "(skipped)";
            continue;
        }

        SampleParser parser = _parserForTopic(topicName);
        if (!parser) {
            qCDebug(DDSLinkLog) << "No parser for topic:" << topicName << "(skipped)";
            continue;
        }

        // Build the DDS topic name with optional namespace prefix
        // PX4 XRCE-DDS publishes as "rt/fmu/out/..." by default
        QString ddsTopicName = prefix.isEmpty()
                                   ? QStringLiteral("rt") + topicName
                                   : prefix + topicName;

        const dds_entity_t reader = _createTypedReader(participant, ddsTopicName, desc);
        if (reader < 0) {
            qCWarning(DDSLinkLog) << "Failed to create reader for:" << ddsTopicName;
            continue;
        }

        ReaderInfo info;
        info.reader = reader;
        info.parser = parser;
        _readers.insert(topicName, info);

        qCInfo(DDSLinkLog) << "Subscribed to" << ddsTopicName << "reader:" << reader;
    }

    qCInfo(DDSLinkLog) << "Subscribed to" << _readers.size() << "of" << topicNames.size() << "topics";
}

QStringList DDSLink::_runDiscovery(dds_entity_t participant)
{
    Q_UNUSED(participant);
    qCDebug(DDSLinkLog) << "Discovery: using mapping-table topics (builtin discovery deferred)";
    return {};
}

dds_entity_t DDSLink::_createTypedReader(dds_entity_t participant,
                                          const QString &topicName,
                                          const dds_topic_descriptor_t *desc)
{
    const QByteArray nameUtf8 = topicName.toUtf8();
    const dds_entity_t topic = dds_create_topic(participant, desc, nameUtf8.constData(),
                                                 nullptr, nullptr);
    if (topic < 0) {
        qCWarning(DDSLinkLog) << "dds_create_topic failed for" << topicName
                              << ":" << dds_strretcode(-topic);
        return topic;
    }

    // Use KEEP_LAST_1 QoS for latest-value semantics (telemetry)
    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

    const dds_entity_t reader = dds_create_reader(participant, topic, qos, nullptr);
    dds_delete_qos(qos);

    if (reader < 0) {
        qCWarning(DDSLinkLog) << "dds_create_reader failed for" << topicName
                              << ":" << dds_strretcode(-reader);
        return reader;
    }

    return reader;
}

QHash<QString, QVariant> DDSLink::_readSample(dds_entity_t reader, const QString &topicName)
{
    auto it = _readers.constFind(topicName);
    if (it == _readers.constEnd() || !it.value().parser) {
        return {};
    }

    void *samples[1] = { nullptr };
    dds_sample_info_t infos[1];

    const dds_return_t rc = dds_take(reader, samples, infos, 1, 1);
    if (rc <= 0 || !infos[0].valid_data) {
        if (samples[0]) {
            dds_return_loan(reader, samples, rc > 0 ? rc : 0);
        }
        return {};
    }

    QHash<QString, QVariant> fields = it.value().parser(samples[0]);
    dds_return_loan(reader, samples, rc);
    return fields;
}

// ---------------------------------------------------------------------------
// Topic descriptor / parser lookup
// ---------------------------------------------------------------------------

const dds_topic_descriptor_t *DDSLink::_descriptorForTopic(const QString &topicName) const
{
    if (topicName.endsWith(QLatin1String("vehicle_attitude")))
        return &px4_msgs_msg_dds__VehicleAttitude__desc;
    if (topicName.endsWith(QLatin1String("vehicle_global_position")))
        return &px4_msgs_msg_dds__VehicleGlobalPosition__desc;
    if (topicName.endsWith(QLatin1String("vehicle_local_position")))
        return &px4_msgs_msg_dds__VehicleLocalPosition__desc;
    if (topicName.endsWith(QLatin1String("battery_status")))
        return &px4_msgs_msg_dds__BatteryStatus__desc;
    if (topicName.endsWith(QLatin1String("vehicle_status")))
        return &px4_msgs_msg_dds__VehicleStatus__desc;

    return nullptr;
}

DDSLink::SampleParser DDSLink::_parserForTopic(const QString &topicName) const
{
    if (topicName.endsWith(QLatin1String("vehicle_attitude")))
        return &DDSLink::_parseVehicleAttitude;
    if (topicName.endsWith(QLatin1String("vehicle_global_position")))
        return &DDSLink::_parseVehicleGlobalPosition;
    if (topicName.endsWith(QLatin1String("vehicle_local_position")))
        return &DDSLink::_parseVehicleLocalPosition;
    if (topicName.endsWith(QLatin1String("battery_status")))
        return &DDSLink::_parseBatteryStatus;
    if (topicName.endsWith(QLatin1String("vehicle_status")))
        return &DDSLink::_parseVehicleStatus;

    return nullptr;
}

// ---------------------------------------------------------------------------
// Per-type sample parsers: struct → QHash<QString, QVariant>
// ---------------------------------------------------------------------------

QHash<QString, QVariant> DDSLink::_parseVehicleAttitude(const void *sample)
{
    const auto *msg = static_cast<const px4_msgs_msg_dds__VehicleAttitude_ *>(sample);
    QHash<QString, QVariant> fields;
    fields.reserve(5);
    fields.insert(QStringLiteral("timestamp"),     QVariant::fromValue(msg->timestamp));
    fields.insert(QStringLiteral("q[0]"),           QVariant(static_cast<double>(msg->q[0])));
    fields.insert(QStringLiteral("q[1]"),           QVariant(static_cast<double>(msg->q[1])));
    fields.insert(QStringLiteral("q[2]"),           QVariant(static_cast<double>(msg->q[2])));
    fields.insert(QStringLiteral("q[3]"),           QVariant(static_cast<double>(msg->q[3])));
    return fields;
}

QHash<QString, QVariant> DDSLink::_parseVehicleGlobalPosition(const void *sample)
{
    const auto *msg = static_cast<const px4_msgs_msg_dds__VehicleGlobalPosition_ *>(sample);
    QHash<QString, QVariant> fields;
    fields.reserve(6);
    fields.insert(QStringLiteral("timestamp"),     QVariant::fromValue(msg->timestamp));
    fields.insert(QStringLiteral("lat"),            QVariant(msg->lat));
    fields.insert(QStringLiteral("lon"),            QVariant(msg->lon));
    fields.insert(QStringLiteral("alt"),            QVariant(static_cast<double>(msg->alt)));
    fields.insert(QStringLiteral("alt_ellipsoid"),  QVariant(static_cast<double>(msg->alt_ellipsoid)));
    fields.insert(QStringLiteral("eph"),            QVariant(static_cast<double>(msg->eph)));
    fields.insert(QStringLiteral("epv"),            QVariant(static_cast<double>(msg->epv)));
    return fields;
}

QHash<QString, QVariant> DDSLink::_parseVehicleLocalPosition(const void *sample)
{
    const auto *msg = static_cast<const px4_msgs_msg_dds__VehicleLocalPosition_ *>(sample);
    QHash<QString, QVariant> fields;
    fields.reserve(10);
    fields.insert(QStringLiteral("timestamp"),     QVariant::fromValue(msg->timestamp));
    fields.insert(QStringLiteral("x"),              QVariant(static_cast<double>(msg->x)));
    fields.insert(QStringLiteral("y"),              QVariant(static_cast<double>(msg->y)));
    fields.insert(QStringLiteral("z"),              QVariant(static_cast<double>(msg->z)));
    fields.insert(QStringLiteral("vx"),             QVariant(static_cast<double>(msg->vx)));
    fields.insert(QStringLiteral("vy"),             QVariant(static_cast<double>(msg->vy)));
    fields.insert(QStringLiteral("vz"),             QVariant(static_cast<double>(msg->vz)));
    fields.insert(QStringLiteral("heading"),        QVariant(static_cast<double>(msg->heading)));
    return fields;
}

QHash<QString, QVariant> DDSLink::_parseBatteryStatus(const void *sample)
{
    const auto *msg = static_cast<const px4_msgs_msg_dds__BatteryStatus_ *>(sample);
    QHash<QString, QVariant> fields;
    fields.reserve(6);
    fields.insert(QStringLiteral("timestamp"),     QVariant::fromValue(msg->timestamp));
    fields.insert(QStringLiteral("voltage_v"),      QVariant(static_cast<double>(msg->voltage_v)));
    fields.insert(QStringLiteral("current_a"),      QVariant(static_cast<double>(msg->current_a)));
    fields.insert(QStringLiteral("remaining"),      QVariant(static_cast<double>(msg->remaining)));
    fields.insert(QStringLiteral("temperature"),    QVariant(static_cast<double>(msg->temperature)));
    fields.insert(QStringLiteral("discharged_mah"), QVariant(static_cast<double>(msg->discharged_mah)));
    return fields;
}

QHash<QString, QVariant> DDSLink::_parseVehicleStatus(const void *sample)
{
    const auto *msg = static_cast<const px4_msgs_msg_dds__VehicleStatus_ *>(sample);
    QHash<QString, QVariant> fields;
    fields.reserve(5);
    fields.insert(QStringLiteral("timestamp"),     QVariant::fromValue(msg->timestamp));
    fields.insert(QStringLiteral("arming_state"),   QVariant(static_cast<int>(msg->arming_state)));
    fields.insert(QStringLiteral("nav_state"),      QVariant(static_cast<int>(msg->nav_state)));
    fields.insert(QStringLiteral("vehicle_type"),   QVariant(static_cast<int>(msg->vehicle_type)));
    fields.insert(QStringLiteral("failsafe"),       QVariant(msg->failsafe));
    return fields;
}

#endif // QGC_ENABLE_DDS
