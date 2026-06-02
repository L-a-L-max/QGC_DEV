#pragma once

#ifdef QGC_ENABLE_DDS

#include "LinkInterface.h"
#include "DDSConfiguration.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "DDSDataInjector.h"

#include <dds/dds.h>

#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtQmlIntegration/QtQmlIntegration>

/// DDS communication link.
/// Connects to PX4 (or other flight controllers) via CycloneDDS.
/// Subscribes to DDS topics defined in the JSON mapping table,
/// reads samples on a poll timer, and injects data into the Fact system.
class DDSLink : public LinkInterface
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

public:
    explicit DDSLink(SharedLinkConfigurationPtr &config, QObject *parent = nullptr);
    ~DDSLink() override;

    void disconnect() override;
    bool isConnected() const override { return _connected; }

    DDSMappingEngine *mappingEngine() { return &_mappingEngine; }
    DDSTransformRegistry *transformRegistry() { return &_transformRegistry; }
    DDSDataInjector *dataInjector() { return &_dataInjector; }

signals:
    void ddsMessageReceived(const QString &topicName,
                            const QHash<QString, QVariant> &fields,
                            quint64 timestampUs);
    void topicsDiscovered();

private slots:
    void _onPollTimer();

private:
    bool _connect() override;
    void _writeBytes(const QByteArray &bytes) override;

    dds_entity_t _createParticipant(int domainId);
    void         _destroyParticipant(dds_entity_t participant);
    void         _subscribeToTopics(dds_entity_t participant, const QStringList &topicNames);
    QStringList  _runDiscovery(dds_entity_t participant);

    dds_entity_t _createTypedReader(dds_entity_t participant,
                                    const QString &topicName,
                                    const dds_topic_descriptor_t *desc);
    QHash<QString, QVariant> _readSample(dds_entity_t reader, const QString &topicName);

    static QHash<QString, QVariant> _parseVehicleAttitude(const void *sample);
    static QHash<QString, QVariant> _parseVehicleGlobalPosition(const void *sample);
    static QHash<QString, QVariant> _parseVehicleLocalPosition(const void *sample);
    static QHash<QString, QVariant> _parseBatteryStatus(const void *sample);
    static QHash<QString, QVariant> _parseVehicleStatus(const void *sample);

    const dds_topic_descriptor_t *_descriptorForTopic(const QString &topicName) const;

    using SampleParser = QHash<QString, QVariant> (*)(const void *);
    SampleParser _parserForTopic(const QString &topicName) const;

    DDSConfiguration *_ddsConfig() const;

    DDSMappingEngine     _mappingEngine;
    DDSTransformRegistry _transformRegistry;
    DDSDataInjector      _dataInjector;

    QTimer               _pollTimer;
    dds_entity_t         _participant = DDS_ENTITY_NIL;
    bool                 _connected   = false;

    struct ReaderInfo {
        dds_entity_t reader = DDS_ENTITY_NIL;
        SampleParser parser = nullptr;
    };

    QHash<QString, ReaderInfo> _readers;
};

#endif // QGC_ENABLE_DDS
