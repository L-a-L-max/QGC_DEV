#ifdef QGC_ENABLE_DDS

#include "DDSManualControlPublisher.h"
#include "ManualControlSetpoint.h"
#include "SettingsManager.h"
#include "AppSettings.h"

#include <QtCore/QDebug>

#include <cmath>
#include <cstring>

Q_LOGGING_CATEGORY(DDSManualControlLog, "DDSManualControlPublisherLog")

DDSManualControlPublisher::DDSManualControlPublisher(QObject *parent)
    : QObject(parent)
{
}

DDSManualControlPublisher::~DDSManualControlPublisher()
{
    deinit();
}

bool DDSManualControlPublisher::init(dds_entity_t participant,
                                     const QString &namespacePrefix)
{
    if (_writer > 0) {
        return true;
    }

    const QString topicName = namespacePrefix + QStringLiteral("fmu/in/manual_control_input");

    _topic = dds_create_topic(
        participant,
        &px4_msgs_msg_dds__ManualControlSetpoint__desc,
        topicName.toUtf8().constData(),
        nullptr, nullptr);

    if (_topic < 0) {
        qCWarning(DDSManualControlLog) << "Failed to create topic:" << topicName;
        _topic = DDS_ENTITY_NIL;
        return false;
    }

    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 2);

    _writer = dds_create_writer(participant, _topic, qos, nullptr);
    dds_delete_qos(qos);

    if (_writer < 0) {
        qCWarning(DDSManualControlLog) << "Failed to create writer:" << topicName;
        _writer = DDS_ENTITY_NIL;
        return false;
    }

    qInfo() << "[DDSManualControl] Writer created on" << topicName;
    return true;
}

void DDSManualControlPublisher::deinit()
{
    if (_writer > 0) {
        dds_delete(_writer);
        _writer = DDS_ENTITY_NIL;
    }
    if (_topic > 0) {
        dds_delete(_topic);
        _topic = DDS_ENTITY_NIL;
    }
}

float DDSManualControlPublisher::_applyJoystickCurve(float input, float deadzone, float expo)
{
    const float sign = (input >= 0.0f) ? 1.0f : -1.0f;
    const float absIn = fabsf(input);

    if (absIn < deadzone) {
        return 0.0f;
    }

    const float normalized = (absIn - deadzone) / (1.0f - deadzone);
    const float curved = (1.0f - expo) * normalized
                       + expo * normalized * normalized * normalized;

    return sign * curved;
}

bool DDSManualControlPublisher::sendManualControl(float roll, float pitch,
                                                  float yaw, float thrust)
{
    if (_writer <= 0) {
        qCWarning(DDSManualControlLog) << "sendManualControl called but writer not ready";
        return false;
    }

    auto *settings = SettingsManager::instance()->appSettings();
    const float maxSpeed = static_cast<float>(settings->virtualJoystickMaxSpeed()->rawValue().toDouble());
    const float deadzone = static_cast<float>(settings->virtualJoystickDeadzone()->rawValue().toDouble());
    const float expo     = static_cast<float>(settings->virtualJoystickExpo()->rawValue().toDouble());

    const float speedScale = maxSpeed / _defaultMaxSpeed;

    roll  = _applyJoystickCurve(roll,  deadzone, expo) * speedScale;
    pitch = _applyJoystickCurve(pitch, deadzone, expo) * speedScale;
    yaw   = _applyJoystickCurve(yaw,   deadzone, expo);

    px4_msgs_msg_dds__ManualControlSetpoint_ msg;
    memset(&msg, 0, sizeof(msg));

    msg.timestamp        = 0;
    msg.timestamp_sample = 0;
    msg.valid       = true;
    msg.data_source = static_cast<uint8_t>(settings->ddsDataSource()->rawValue().toUInt());

    // QGC: thrust [0,1] (center=0.5).  PX4: throttle [-1,1] (center=0).
    const float throttle = (thrust * 2.0f) - 1.0f;

    msg.roll     = roll;
    msg.pitch    = pitch;
    msg.yaw      = yaw;
    msg.throttle = throttle;

    msg.sticks_moving = (fabsf(roll)  > 0.01f || fabsf(pitch) > 0.01f ||
                         fabsf(yaw)   > 0.01f || fabsf(throttle) > 0.05f);

    const dds_return_t rc = dds_write(_writer, &msg);

    if (_sendCount++ % 25 == 0) {
        dds_instance_handle_t ihs[16];
        const dds_return_t nMatched = dds_get_matched_subscriptions(_writer, ihs, 16);
        qWarning() << "[DDSManualControl] send: r=" << roll
                   << "p=" << pitch << "y=" << yaw
                   << "t(raw)=" << thrust << "t(mapped)=" << throttle
                   << "src=" << msg.data_source
                   << "spd=" << maxSpeed << "dz=" << deadzone << "expo=" << expo
                   << "rc=" << rc << "matched=" << nMatched;
    }

    return rc == DDS_RETCODE_OK;
}

#endif // QGC_ENABLE_DDS
