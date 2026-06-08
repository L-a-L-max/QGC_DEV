#ifdef QGC_ENABLE_DDS

#include "DDSHeartbeatPublisher.h"
#include "TelemetryStatus.h"

#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include <cstring>

Q_LOGGING_CATEGORY(DDSHeartbeatPublisherLog, "DDSHeartbeatPublisherLog")

DDSHeartbeatPublisher::DDSHeartbeatPublisher(QObject *parent)
    : QObject(parent)
{
    connect(&_timer, &QTimer::timeout, this, &DDSHeartbeatPublisher::_sendHeartbeat);
}

DDSHeartbeatPublisher::~DDSHeartbeatPublisher()
{
    deinit();
}

bool DDSHeartbeatPublisher::init(dds_entity_t participant,
                                  const QString &namespacePrefix,
                                  int intervalMs)
{
    if (_writer > 0) {
        return true;
    }

    const QString topicName = namespacePrefix + QStringLiteral("fmu/in/telemetry_status");

    _topic = dds_create_topic(
        participant,
        &px4_msgs_msg_dds__TelemetryStatus__desc,
        topicName.toUtf8().constData(),
        nullptr, nullptr);

    if (_topic < 0) {
        qCWarning(DDSHeartbeatPublisherLog) << "Failed to create topic:" << topicName;
        return false;
    }

    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

    _writer = dds_create_writer(participant, _topic, qos, nullptr);
    dds_delete_qos(qos);

    if (_writer < 0) {
        qCWarning(DDSHeartbeatPublisherLog) << "Failed to create writer:" << topicName;
        _writer = DDS_ENTITY_NIL;
        return false;
    }

    _timer.start(intervalMs);
    qInfo() << "[DDSHeartbeat] Started GCS heartbeat every" << intervalMs
            << "ms on" << topicName;
    return true;
}

void DDSHeartbeatPublisher::deinit()
{
    _timer.stop();

    if (_writer > 0) {
        dds_delete(_writer);
        _writer = DDS_ENTITY_NIL;
    }
    if (_topic > 0) {
        dds_delete(_topic);
        _topic = DDS_ENTITY_NIL;
    }
}

void DDSHeartbeatPublisher::_sendHeartbeat()
{
    if (_writer <= 0) {
        return;
    }

    px4_msgs_msg_dds__TelemetryStatus_ msg;
    memset(&msg, 0, sizeof(msg));

    msg.timestamp = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
    msg.heartbeat_type_gcs = true;

    dds_write(_writer, &msg);
}

#endif // QGC_ENABLE_DDS
