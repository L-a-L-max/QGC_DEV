#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>

#include <dds/dds.h>

/// Publishes ManualControlSetpoint messages to PX4 via DDS, enabling
/// the QGC virtual joystick to control the vehicle through the DDS link.
///
/// Supports configurable max-speed scaling, deadzone, and exponential
/// response curves via AppSettings.
class DDSManualControlPublisher : public QObject
{
    Q_OBJECT

public:
    explicit DDSManualControlPublisher(QObject *parent = nullptr);
    ~DDSManualControlPublisher() override;

    bool init(dds_entity_t participant, const QString &namespacePrefix);
    void deinit();

    bool isReady() const { return _writer > 0; }

    /// Send manual control values to PX4.
    /// roll/pitch/yaw: [-1, 1].  thrust: [0, 1] (QGC convention, mapped to [-1,1] internally).
    /// Deadzone, expo, and speed scaling from settings are applied internally.
    bool sendManualControl(float roll, float pitch, float yaw, float thrust);

private:
    static float _applyJoystickCurve(float input, float deadzone, float expo);

    dds_entity_t _writer = DDS_ENTITY_NIL;
    dds_entity_t _topic  = DDS_ENTITY_NIL;
    uint64_t     _sendCount = 0;

    static constexpr float _defaultMaxSpeed = 12.0f;
};

#endif // QGC_ENABLE_DDS
