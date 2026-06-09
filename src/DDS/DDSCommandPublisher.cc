#ifdef QGC_ENABLE_DDS

#include "DDSCommandPublisher.h"
#include "VehicleCommand.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>

#include <cstring>

Q_LOGGING_CATEGORY(DDSCommandPublisherLog, "DDSCommandPublisherLog")

DDSCommandPublisher::DDSCommandPublisher(QObject *parent)
    : QObject(parent)
{
}

DDSCommandPublisher::~DDSCommandPublisher()
{
    deinit();
}

bool DDSCommandPublisher::init(dds_entity_t participant, const QString &namespacePrefix)
{
    if (_writer > 0) {
        return true;
    }

    _participant = participant;
    _topicName = namespacePrefix + QStringLiteral("fmu/in/vehicle_command");

    _topic = dds_create_topic(
        participant,
        &px4_msgs_msg_dds__VehicleCommand__desc,
        _topicName.toUtf8().constData(),
        nullptr, nullptr);

    if (_topic < 0) {
        qCWarning(DDSCommandPublisherLog) << "Failed to create topic:" << _topicName
                                          << "error:" << dds_strretcode(-_topic);
        _topic = DDS_ENTITY_NIL;
        return false;
    }

    // BEST_EFFORT + VOLATILE — matches PX4's XRCE-DDS reader QoS exactly.
    // Same QoS as DDSHeartbeatPublisher, which delivers to all 3 namespaces.
    // RELIABLE writers fail to deliver to non-default PX4 instances because
    // the XRCE-DDS Agent's built-in RTPS does not correctly route RELIABLE
    // data to multiple BEST_EFFORT readers of the same type.
    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

    _writer = dds_create_writer(participant, _topic, qos, nullptr);
    dds_delete_qos(qos);

    if (_writer < 0) {
        qCWarning(DDSCommandPublisherLog) << "Failed to create writer:" << _topicName;
        _writer = DDS_ENTITY_NIL;
        return false;
    }

    qInfo() << "[DDSCommandPublisher] Initialized writer on" << _topicName
            << "writer=" << _writer;
    return true;
}

void DDSCommandPublisher::deinit()
{
    if (_writer > 0) {
        dds_delete(_writer);
        _writer = DDS_ENTITY_NIL;
    }
    if (_topic > 0) {
        dds_delete(_topic);
        _topic = DDS_ENTITY_NIL;
    }
    _participant = DDS_ENTITY_NIL;
}

int DDSCommandPublisher::matchedSubscriptionCount() const
{
    if (_writer <= 0) return 0;
    dds_instance_handle_t handles[10];
    const int n = dds_get_matched_subscriptions(_writer, handles, 10);
    return (n > 0) ? n : 0;
}

bool DDSCommandPublisher::sendCommand(uint32_t command,
                                      float param1, float param2,
                                      float param3, float param4,
                                      double param5, double param6,
                                      float param7,
                                      uint8_t targetSystem,
                                      uint8_t targetComponent)
{
    if (_writer <= 0) {
        const QString reason = QStringLiteral("Publisher not initialized");
        qCWarning(DDSCommandPublisherLog) << "Cannot send command" << command << ":" << reason;
        emit commandFailed(command, reason);
        return false;
    }

    // PX4's DDS interface does not support MAV_CMD_REQUEST_MESSAGE (512).
    if (command == 512) {
        return true;
    }

    px4_msgs_msg_dds__VehicleCommand_ msg;
    memset(&msg, 0, sizeof(msg));

    msg.timestamp = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
    msg.param1 = param1;
    msg.param2 = param2;
    msg.param3 = param3;
    msg.param4 = param4;
    msg.param5 = param5;
    msg.param6 = param6;
    msg.param7 = param7;
    msg.command = command;
    Q_UNUSED(targetSystem);
    msg.target_system = 1;
    msg.target_component = targetComponent;
    msg.source_system = 255;
    msg.source_component = 190;
    msg.confirmation = 0;
    msg.from_external = true;

    const dds_return_t rc = dds_write(_writer, &msg);
    if (rc != DDS_RETCODE_OK) {
        const QString reason = QString::fromLatin1(dds_strretcode(-rc));
        qCWarning(DDSCommandPublisherLog) << "dds_write failed for command" << command
                                          << ":" << reason;
        emit commandFailed(command, reason);
        return false;
    }

    const int subs = matchedSubscriptionCount();
    qInfo() << "[DDSCommandPublisher] Sent command" << command
            << "on" << _topicName
            << "matched=" << subs
            << "writer=" << _writer
            << "p1=" << param1 << "p2=" << param2 << "p3=" << param3
            << "p7=" << param7
            << "target=" << msg.target_system << "/" << msg.target_component;
    if (subs == 0) {
        qCWarning(DDSCommandPublisherLog) << "WARNING: command" << command
            << "written to" << _topicName
            << "but writer has 0 matched subscriptions — PX4 will NOT receive it";
    }
    emit commandSent(command, param1);
    return true;
}

#endif // QGC_ENABLE_DDS
