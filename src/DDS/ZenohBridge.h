#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>
#include <QtCore/QString>

/// Manages the zenoh-bridge-dds subprocess on Android.
/// On non-Android platforms this is a no-op stub so that DDSLink
/// compiles without #ifdef clutter around every call site.
class ZenohBridge : public QObject
{
    Q_OBJECT

public:
    explicit ZenohBridge(QObject *parent = nullptr);
    ~ZenohBridge() override;

    /// Start the bridge subprocess connecting to the given Zenoh endpoint.
    /// @param endpoint  e.g. "tcp/192.168.1.100:7447"
    /// @param domainId  DDS domain (must match DDSLink)
    /// @return true on success (or if already running)
    bool start(const QString &endpoint, int domainId);

    /// Stop the bridge subprocess.
    void stop();

    /// @return true if the bridge process is alive.
    bool isRunning() const;

    /// @return human-readable status string.
    QString statusText() const;

    /// @return last error message, empty if none.
    QString lastError() const;

signals:
    void started();
    void stopped();
    void errorOccurred(const QString &error);
};

#endif // QGC_ENABLE_DDS
