#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>

#include <dds/dds.h>

/// Publishes ManualControlSetpoint messages to PX4 via DDS, enabling
/// the QGC virtual joystick to control the vehicle through the DDS link.
///
/// PX4 subscribes to fmu/in/manual_control_input and uses it as a
/// joystick/RC source when in position or manual flight modes.
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
    /// All axes are in the range [-1, 1].
    bool sendManualControl(float roll, float pitch, float yaw, float throttle);

private:
    dds_entity_t _writer = DDS_ENTITY_NIL;
    dds_entity_t _topic  = DDS_ENTITY_NIL;
    uint64_t     _sendCount = 0;
};

#endif // QGC_ENABLE_DDS
