#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QVariant>

class DDSDataInjector;
class DDSLink;
class Vehicle;

/// Monitors DDS telemetry to create and manage a Vehicle when no MAVLink is
/// available.  When vehicle_status data arrives (nav_state, arming_state,
/// vehicle_type), DDSVehicleManager creates a lightweight Vehicle object and
/// attaches the DDSDataInjector so telemetry flows into the Fact system.
///
/// Lifecycle:
///   1. DDSLink emits ddsMessageReceived for /fmu/out/vehicle_status
///   2. DDSVehicleManager creates a Vehicle via MultiVehicleManager API
///   3. DDSDataInjector::setVehicle() is called
///   4. All subsequent DDS data is injected into the Vehicle's Facts
class DDSVehicleManager : public QObject
{
    Q_OBJECT

public:
    explicit DDSVehicleManager(DDSLink *link, QObject *parent = nullptr);
    ~DDSVehicleManager() override;

    /// Whether a DDS-only Vehicle has been created.
    bool hasVehicle() const { return _vehicleCreated; }

public slots:
    void onDDSMessage(const QString &topicName,
                      const QHash<QString, QVariant> &fields,
                      quint64 timestampUs);

private:
    void _createVehicle(int vehicleType);
    void _emitSyntheticHeartbeat();
    static uint32_t _navStateToCustomMode(int navState);
    static int _nextVehicleId();

    DDSLink *_link = nullptr;
    bool     _vehicleCreated = false;
    QTimer   _heartbeatTimer;
    int      _vehicleId = 0;
    int      _mavType = 0;
};

#endif // QGC_ENABLE_DDS
