#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include <dds/dds.h>

/// Discovers DDS namespaces on a given domain by inspecting builtin
/// publication endpoints.  A temporary participant is created for discovery
/// and destroyed when scanning stops.
class DDSDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit DDSDiscovery(QObject *parent = nullptr);
    ~DDSDiscovery() override;

    /// Start scanning for namespaces on the given DDS domain.
    /// Emits namespacesUpdated() whenever the set of discovered
    /// namespaces changes.
    void startDiscovery(int domainId);

    /// Stop scanning and release the temporary participant.
    void stopDiscovery();

    /// Whether a scan is currently active.
    bool isRunning() const { return _running; }

    /// Currently discovered namespace list (may grow while running).
    QStringList discoveredNamespaces() const { return _namespaces; }

signals:
    void namespacesUpdated(const QStringList &namespaces);
    void discoveryError(const QString &message);
    void runningChanged();

private slots:
    void _pollBuiltinTopics();

private:
    static QStringList _extractNamespaces(dds_entity_t participant);

    dds_entity_t _participant = DDS_ENTITY_NIL;
    QTimer       _pollTimer;
    QStringList  _namespaces;
    bool         _running = false;
};

#endif // QGC_ENABLE_DDS
