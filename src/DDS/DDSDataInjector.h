#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

class DDSMappingEngine;
class DDSMissionManager;
class DDSTransformRegistry;
class FactGroup;
class BatteryFactGroup;
class Vehicle;

/// @file DDSDataInjector.h
/// @brief Injects DDS telemetry data into QGC's Fact system.
///
/// DDSDataInjector is the bridge between raw DDS messages and QGC's data model.
/// It receives parsed DDS fields, looks up the mapping in DDSMappingEngine,
/// applies transforms via DDSTransformRegistry, and calls Fact::setRawValue()
/// on the target Vehicle's FactGroups.
///
/// Additionally handles:
///   - Vehicle coordinate updates (lat/lon/alt → Vehicle::setCoordinateFromDDS)
///   - Armed state and flight mode (direct Vehicle property updates)
///   - Battery FactGroup (via BatteryFactGroupListModel)

class DDSDataInjector : public QObject
{
    Q_OBJECT

public:
    explicit DDSDataInjector(DDSMappingEngine *engine,
                             DDSTransformRegistry *transforms,
                             QObject *parent = nullptr);
    ~DDSDataInjector() override;

    /// Set the target Vehicle whose Facts will be updated.
    void setVehicle(Vehicle *vehicle);

    /// Set the mission manager to receive position/home updates.
    void setMissionManager(DDSMissionManager *mgr) { _missionManager = mgr; }

    /// Statistics: total DDS messages processed.
    quint64 messagesProcessed() const { return _messagesProcessed; }

    /// Statistics: total Fact values written.
    quint64 factsUpdated() const { return _factsUpdated; }

    /// Statistics: total unmapped topics/fields skipped.
    quint64 unmappedSkipped() const { return _unmappedSkipped; }

    /// Current nav_state from vehicle_status (for synthetic heartbeat)
    int navState() const { return _navState; }

    /// Current arming_state from vehicle_status (for synthetic heartbeat)
    int armingState() const { return _armingState; }

public slots:
    /// Process a received DDS message. Called from DDSLink::ddsMessageReceived signal.
    /// @param topicName   Fully qualified DDS topic name
    /// @param fields      Parsed field name → value pairs from the DDS sample
    /// @param timestampUs Timestamp in microseconds
    void onDDSMessage(const QString &topicName,
                      const QHash<QString, QVariant> &fields,
                      quint64 timestampUs);

signals:
    /// Emitted when a Fact value is updated from DDS data.
    void factUpdated(const QString &factGroupPath, const QString &factName, const QVariant &value);

    /// Emitted when a command ACK is received from PX4.
    void commandAckReceived(uint32_t command, uint8_t result, uint8_t targetSystem);

private:
    void _injectField(const QString &factGroupPath,
                      const QString &factName,
                      const QVariant &value);

    FactGroup *_resolveFactGroup(const QString &path) const;

    void _updateVehicleCoordinate(const QHash<QString, QVariant> &fields);
    void _updateVehicleState(const QHash<QString, QVariant> &fields);
    void _updateHomePosition(const QHash<QString, QVariant> &fields);
    void _updateLandDetected(const QHash<QString, QVariant> &fields);
    void _updateReadyToFly(const QHash<QString, QVariant> &fields);
    void _handleCommandAck(const QHash<QString, QVariant> &fields);
    void _ensureBatteryExists();

    DDSMappingEngine     *_mappingEngine    = nullptr;
    DDSTransformRegistry *_transformRegistry = nullptr;
    Vehicle              *_vehicle          = nullptr;

    quint64 _messagesProcessed = 0;
    quint64 _factsUpdated      = 0;
    quint64 _unmappedSkipped   = 0;

    int  _navState     = 0;
    int  _armingState  = 0;
    bool _batteryCreated = false;
    bool _readyToFlySet = false;
    bool _homeSetFromArm = false;
    double _lastLat = 0.0;
    double _lastLon = 0.0;
    double _lastAlt = 0.0;
    double _homeAltMSL = qQNaN();
    bool   _homeAltValid = false;
    DDSMissionManager *_missionManager = nullptr;

    void _updateDerivedAltitude();
    void _updateTimeToHome();
};

#endif // QGC_ENABLE_DDS
