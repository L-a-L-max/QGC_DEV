#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

class DDSMappingEngine;
class DDSTransformRegistry;
class FactGroup;
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

    /// Statistics: total DDS messages processed.
    quint64 messagesProcessed() const { return _messagesProcessed; }

    /// Statistics: total Fact values written.
    quint64 factsUpdated() const { return _factsUpdated; }

    /// Statistics: total unmapped topics/fields skipped.
    quint64 unmappedSkipped() const { return _unmappedSkipped; }

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

private:
    void _injectField(const QString &factGroupPath,
                      const QString &factName,
                      const QVariant &value);

    FactGroup *_resolveFactGroup(const QString &path) const;

    void _updateVehicleCoordinate(const QHash<QString, QVariant> &fields);
    void _updateVehicleState(const QHash<QString, QVariant> &fields);

    DDSMappingEngine     *_mappingEngine    = nullptr;
    DDSTransformRegistry *_transformRegistry = nullptr;
    Vehicle              *_vehicle          = nullptr;

    quint64 _messagesProcessed = 0;
    quint64 _factsUpdated      = 0;
    quint64 _unmappedSkipped   = 0;
};

#endif // QGC_ENABLE_DDS
