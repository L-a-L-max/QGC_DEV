#pragma once

#ifdef QGC_ENABLE_DDS

#include "LinkConfiguration.h"

#include <dds/dds.h>

#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

class DDSDiscovery;

/// Configuration for a DDS communication link.
/// Holds user-editable settings: DDS domain ID, vendor mapping file,
/// namespace prefix, and auto-discovery toggle.
class DDSConfiguration : public LinkConfiguration
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(int      domainId              READ domainId             WRITE setDomainId             NOTIFY domainIdChanged)
    Q_PROPERTY(QString  vendorMapping         READ vendorMapping        WRITE setVendorMapping        NOTIFY vendorMappingChanged)
    Q_PROPERTY(QString  namespacePrefix       READ namespacePrefix      WRITE setNamespacePrefix      NOTIFY namespacePrefixChanged)
    Q_PROPERTY(bool     autoDiscover          READ autoDiscover         WRITE setAutoDiscover         NOTIFY autoDiscoverChanged)
    Q_PROPERTY(QStringList discoveredNamespaces READ discoveredNamespaces                             NOTIFY discoveredNamespacesChanged)
    Q_PROPERTY(bool     discovering           READ discovering                                        NOTIFY discoveringChanged)

public:
    explicit DDSConfiguration(const QString &name, QObject *parent = nullptr);
    DDSConfiguration(const DDSConfiguration *copy, QObject *parent = nullptr);
    ~DDSConfiguration() override;

    int domainId() const { return _domainId; }
    void setDomainId(int id);

    QString vendorMapping() const { return _vendorMapping; }
    void setVendorMapping(const QString &mapping);

    QString namespacePrefix() const { return _namespacePrefix; }
    void setNamespacePrefix(const QString &ns);

    bool autoDiscover() const { return _autoDiscover; }
    void setAutoDiscover(bool enabled);

    QStringList discoveredNamespaces() const { return _discoveredNamespaces; }
    bool discovering() const { return _discovering; }

    /// Start namespace discovery on the current domainId.
    Q_INVOKABLE void startDiscovery();

    /// Stop an active discovery scan.
    Q_INVOKABLE void stopDiscovery();

    /// Release the discovery participant for reuse by DDSLink.
    /// Returns DDS_ENTITY_NIL if no participant is available.
    dds_entity_t takeDiscoveryParticipant();

    // LinkConfiguration overrides
    LinkType type() const override { return TypeDDS; }
    void copyFrom(const LinkConfiguration *source) override;
    void loadSettings(QSettings &settings, const QString &root) override;
    void saveSettings(QSettings &settings, const QString &root) const override;
    QString settingsURL() const override { return QStringLiteral("DDSSettings.qml"); }
    QString settingsTitle() const override { return tr("DDS Link Settings"); }

signals:
    void domainIdChanged();
    void vendorMappingChanged();
    void namespacePrefixChanged();
    void autoDiscoverChanged();
    void discoveredNamespacesChanged();
    void discoveringChanged();

private slots:
    void _onNamespacesUpdated(const QStringList &namespaces);

private:
    int         _domainId        = 0;
    QString     _vendorMapping;
    QString     _namespacePrefix;
    bool        _autoDiscover    = true;
    QStringList _discoveredNamespaces;
    bool        _discovering     = false;

    DDSDiscovery *_discovery = nullptr;
};

#endif // QGC_ENABLE_DDS
