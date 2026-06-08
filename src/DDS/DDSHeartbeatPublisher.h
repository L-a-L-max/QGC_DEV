#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <dds/dds.h>

/// Periodically publishes a TelemetryStatus message with heartbeat_type_gcs=true
/// to rt/fmu/in/telemetry_status so that PX4 Commander recognises QGC as a
/// connected GCS and clears the "No connection to the GCS" pre-arm check.
class DDSHeartbeatPublisher : public QObject
{
    Q_OBJECT

public:
    explicit DDSHeartbeatPublisher(QObject *parent = nullptr);
    ~DDSHeartbeatPublisher() override;

    /// Create the DDS writer and start the periodic timer.
    /// @param participant  CycloneDDS participant entity
    /// @param namespacePrefix  Topic namespace prefix (e.g. "rt/")
    /// @param intervalMs  Heartbeat interval in milliseconds (default 1000)
    /// @return true if writer was successfully created
    bool init(dds_entity_t participant,
              const QString &namespacePrefix,
              int intervalMs = 1000);

    void deinit();

    bool isRunning() const { return _timer.isActive(); }

private slots:
    void _sendHeartbeat();

private:
    dds_entity_t _writer = DDS_ENTITY_NIL;
    dds_entity_t _topic  = DDS_ENTITY_NIL;
    QTimer       _timer;
};

#endif // QGC_ENABLE_DDS
