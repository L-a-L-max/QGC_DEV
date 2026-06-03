#pragma once

#ifdef QGC_ENABLE_DDS

#include "LinkInterface.h"
#include "DDSConfiguration.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "DDSDataInjector.h"
#include "DDSTypeRegistry.h"
#include "DDSVehicleManager.h"

#include <dds/dds.h>

#include <QtCore/QHash>
#include <QtCore/QSet>
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
    QHash<QString, QVariant> _readSample(dds_entity_t reader, const QString &topicName);

    DDSConfiguration *_ddsConfig() const;

    DDSMappingEngine     _mappingEngine;
    DDSTransformRegistry _transformRegistry;
    DDSTypeRegistry      _typeRegistry;
    DDSDataInjector      _dataInjector;
    DDSVehicleManager    _vehicleManager;

    QTimer               _pollTimer;
    dds_entity_t         _participant = DDS_ENTITY_NIL;
    bool                 _connected   = false;

    struct ReaderInfo {
        dds_entity_t reader = DDS_ENTITY_NIL;
        DDSFieldExtractorFunc extractor;
    };

    QHash<QString, ReaderInfo> _readers;
    QSet<QString>              _receivedTopics;  ///< tracks first-sample logging per topic
};

#endif // QGC_ENABLE_DDS
