#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>
#include <QtCore/QString>
#include <dds/dds.h>

/// DDSCommandPublisher creates a DDS writer for the vehicle_command topic
/// and publishes commands to PX4 via DDS.
///
/// Uses BEST_EFFORT + VOLATILE QoS to match PX4's XRCE-DDS reader QoS.
/// Writer is created immediately in init() and kept alive (same pattern as
/// DDSHeartbeatPublisher, which works for all multi-vehicle namespaces).
class DDSCommandPublisher : public QObject
{
    Q_OBJECT

public:
    explicit DDSCommandPublisher(QObject *parent = nullptr);
    ~DDSCommandPublisher() override;

    /// Initialize topic and writer immediately.
    bool init(dds_entity_t participant, const QString &namespacePrefix);

    /// Cleanup everything (writer + topic).
    void deinit();

    /// Whether the publisher has been initialized (topic created).
    bool isReady() const { return _topic > 0 && _participant > 0; }

    /// Number of matched subscriptions (PX4 readers) for diagnostics.
    int matchedSubscriptionCount() const;

    /// Topic name this writer publishes to.
    QString topicName() const { return _topicName; }

    /// Send a vehicle command to PX4.
    bool sendCommand(uint32_t command,
                     float param1 = 0.0f, float param2 = 0.0f,
                     float param3 = 0.0f, float param4 = 0.0f,
                     double param5 = 0.0, double param6 = 0.0,
                     float param7 = 0.0f,
                     uint8_t targetSystem = 1,
                     uint8_t targetComponent = 1);

signals:
    void commandSent(uint32_t command, float param1);
    void commandFailed(uint32_t command, const QString &reason);

private:
    dds_entity_t _writer      = DDS_ENTITY_NIL;
    dds_entity_t _topic       = DDS_ENTITY_NIL;
    dds_entity_t _participant = DDS_ENTITY_NIL;
    QString      _topicName;
};

#endif // QGC_ENABLE_DDS
