#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>
#include <QtCore/QString>

#include <dds/dds.h>

/// DDSCommandPublisher creates a DDS writer for the vehicle_command topic
/// and publishes commands to PX4 via DDS.
///
/// This enables QGC to send arm/disarm, mode change, takeoff/land commands
/// through the DDS link instead of MAVLink.
class DDSCommandPublisher : public QObject
{
    Q_OBJECT

public:
    explicit DDSCommandPublisher(QObject *parent = nullptr);
    ~DDSCommandPublisher() override;

    /// Initialize the DDS writer. Must be called after participant is created.
    /// @param participant  CycloneDDS participant entity
    /// @param namespacePrefix  Topic namespace prefix (e.g. "rt/px4_1/")
    /// @return true if writer was successfully created
    bool init(dds_entity_t participant, const QString &namespacePrefix);

    /// Cleanup the writer.
    void deinit();

    /// Whether the writer is initialized and ready to send.
    bool isReady() const { return _writer > 0; }

    /// Send a vehicle command to PX4.
    /// @param command     MAV_CMD command ID
    /// @param param1-7    Command parameters
    /// @param targetSystem   Target system ID (default: 1 = PX4)
    /// @param targetComponent  Target component ID (default: 1 = autopilot)
    /// @return true if the write succeeded
    bool sendCommand(uint32_t command,
                     float param1 = 0.0f, float param2 = 0.0f,
                     float param3 = 0.0f, float param4 = 0.0f,
                     double param5 = 0.0, double param6 = 0.0,
                     float param7 = 0.0f,
                     uint8_t targetSystem = 1,
                     uint8_t targetComponent = 1);

signals:
    /// Emitted when a command is successfully written to DDS.
    void commandSent(uint32_t command, float param1);

    /// Emitted when a command write fails.
    void commandFailed(uint32_t command, const QString &reason);

private:
    dds_entity_t _writer = DDS_ENTITY_NIL;
    dds_entity_t _topic  = DDS_ENTITY_NIL;
};

#endif // QGC_ENABLE_DDS
