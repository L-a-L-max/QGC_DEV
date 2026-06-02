#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QTimer>

#include "DDSConfiguration.h"

class DDSMappingEngine;
class DDSDataInjector;
class DDSTransformRegistry;
class Vehicle;

/// @file DDSLink.h
/// @brief DDS communication link for QGC.
///
/// DDSLink subscribes to PX4 DDS topics and injects telemetry data into QGC's
/// Fact system via DDSMappingEngine + DDSDataInjector. It sits alongside the
/// normal MAVLink link; when configured as the sole link, DDS carries all data.
///
/// Lifecycle:
///   1. User creates DDSConfiguration (domain ID, vendor mapping, namespace)
///   2. LinkManager calls _connect() → creates DDS participant + readers
///   3. _pollTimer fires periodically → reads DDS samples → emits ddsMessageReceived
///   4. DDSDataInjector receives signal → maps via MappingEngine → writes Fact values
///   5. disconnect() tears down participant
///
/// Thread safety: DDS callbacks and polling happen on the Qt main thread via
/// QTimer. No separate DDS thread is needed for the poll-based approach.

class DDSLink : public LinkConfiguration  // NOTE: In QGC integration, inherit LinkInterface
{
    Q_OBJECT

public:
    explicit DDSLink(DDSConfiguration *config, QObject *parent = nullptr);
    ~DDSLink() override;

    /// Connect to the DDS network. Creates DDS participant, discovers topics,
    /// and begins polling for samples.
    bool connectLink();

    /// Disconnect from DDS. Destroys all readers and the participant.
    void disconnectLink();

    bool isConnected() const { return _connected; }

    DDSMappingEngine    *mappingEngine() const { return _mappingEngine; }
    DDSDataInjector     *dataInjector() const { return _dataInjector; }
    DDSTransformRegistry *transformRegistry() const { return _transformRegistry; }

    /// Attach a Vehicle to this link so DDSDataInjector can write to its Facts.
    void setVehicle(Vehicle *vehicle);

signals:
    /// Emitted when a raw DDS sample is received.
    /// @param topicName  Fully qualified DDS topic name (e.g. "/fmu/out/vehicle_attitude")
    /// @param fields     Key-value pairs extracted from the DDS sample
    /// @param timestampUs Timestamp in microseconds
    void ddsMessageReceived(const QString &topicName,
                            const QHash<QString, QVariant> &fields,
                            quint64 timestampUs);

    /// Emitted when DDS topic discovery finds topics on the network.
    void topicsDiscovered(const QStringList &topics);

    void connectedChanged(bool connected);

private slots:
    void _onPollTimer();

private:
    bool _createParticipant();
    void _destroyParticipant();
    void _subscribeToTopics(const QStringList &topicNames);
    QStringList _runDiscovery();
    QHash<QString, QVariant> _readSample(const QString &topicName);

    DDSConfiguration     *_config           = nullptr;
    DDSMappingEngine     *_mappingEngine    = nullptr;
    DDSDataInjector      *_dataInjector     = nullptr;
    DDSTransformRegistry *_transformRegistry = nullptr;

    QTimer  _pollTimer;
    bool    _connected = false;
    int     _participantHandle = -1;  // CycloneDDS: dds_entity_t

    // topic name → reader handle
    QHash<QString, int> _readers;
    mutable QMutex _mutex;
};

#endif // QGC_ENABLE_DDS
