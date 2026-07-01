#pragma once

#ifdef QGC_ENABLE_ZENOH

#include "LinkInterface.h"
#include "ZenohConfiguration.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "DDSDataInjector.h"
#include "DDSTypeRegistry.h"

#include <zenoh-pico.h>

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtQmlIntegration/QtQmlIntegration>

/// Zenoh communication link for PX4 telemetry via zenoh-bridge-dds.
///
/// Connects to a Zenoh router/peer, subscribes to key-expressions that
/// match DDS topics, parses CDR-encoded payloads using the IDL-generated
/// type structs, and injects data into QGC's Fact system through
/// DDSDataInjector.
///
/// Typical deployment:
///   PX4 → MicroXRCE-DDS Agent → CycloneDDS → zenoh-bridge-dds
///     → Zenoh network → ZenohLink (QGC mobile)
class ZenohLink : public LinkInterface
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

public:
    explicit ZenohLink(SharedLinkConfigurationPtr &config, QObject *parent = nullptr);
    ~ZenohLink() override;

    void disconnect() override;
    bool isConnected() const override { return _connected; }

    DDSMappingEngine *mappingEngine() { return &_mappingEngine; }
    DDSDataInjector *dataInjector() { return &_dataInjector; }

    /// Publish raw CDR bytes to a Zenoh key-expression.
    bool publishCdr(const QString &keyExpr, const void *data, size_t len);

signals:
    void zenohMessageReceived(const QString &topicName,
                              const QHash<QString, QVariant> &fields,
                              quint64 timestampUs);

private slots:
    void _onPollTimer();
    void _onZenohMessage(const QString &topicName,
                         const QHash<QString, QVariant> &fields,
                         quint64 timestampUs);

private:
    bool _connect() override;
    void _writeBytes(const QByteArray &bytes) override;

    ZenohConfiguration *_zenohConfig() const;

    bool _openSession();
    void _closeSession();
    void _subscribeToTopics(const QStringList &topicNames);

    QHash<QString, QVariant> _parseCdrPayload(const QString &topicName,
                                              const uint8_t *data, size_t len);

    void _tryCreateVehicle(const QHash<QString, QVariant> &fields);
    void _emitSyntheticHeartbeat();
    static uint32_t _navStateToCustomMode(int navState);

    DDSMappingEngine     _mappingEngine;
    DDSTransformRegistry _transformRegistry;
    DDSTypeRegistry      _typeRegistry;
    DDSDataInjector      _dataInjector;

    QTimer               _pollTimer;
    QTimer               _heartbeatTimer;
    bool                 _connected = false;
    bool                 _sessionOpen = false;
    bool                 _vehicleCreated = false;
    int                  _vehicleId = 0;
    int                  _mavType = 0;

    z_owned_session_t    _session;

    struct SubInfo {
        z_owned_subscriber_t subscriber;
    };

    struct PendingSample {
        QString topicName;
        QHash<QString, QVariant> fields;
    };

    QMutex _pendingMutex;
    QList<PendingSample> _pendingSamples;

    QHash<QString, SubInfo> _subscribers;
    QSet<QString> _receivedTopics;

    static void _zenohDataCallback(z_loaned_sample_t *sample, void *ctx);

    struct CallbackContext {
        ZenohLink *link;
        QString topicName;
        DDSFieldExtractorFunc extractor;
    };
    QList<CallbackContext *> _callbackContexts;
};

#endif // QGC_ENABLE_ZENOH
