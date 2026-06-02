#pragma once

#ifdef QGC_ENABLE_DDS

#include "LinkInterface.h"
#include "DDSConfiguration.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "DDSDataInjector.h"

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
    // LinkInterface overrides
    bool _connect() override;
    void _writeBytes(const QByteArray &bytes) override;

    // CycloneDDS operations (stubs until P1 real implementation)
    int  _createParticipant(int domainId);
    void _destroyParticipant(int participant);
    void _subscribeToTopics(int participant, const QStringList &topicNames);
    QStringList _runDiscovery(int participant);
    QHash<QString, QVariant> _readSample(int reader);

    DDSConfiguration *_ddsConfig() const;

    DDSMappingEngine     _mappingEngine;
    DDSTransformRegistry _transformRegistry;
    DDSDataInjector      _dataInjector;

    QTimer               _pollTimer;
    int                  _participant = -1;
    bool                 _connected   = false;

    QHash<QString, int>  _readers;
};

#endif // QGC_ENABLE_DDS
