#pragma once

#ifdef QGC_ENABLE_DDS

#include "LinkConfiguration.h"

#include <QtQmlIntegration/QtQmlIntegration>

/// Configuration for a DDS communication link.
/// Holds user-editable settings: DDS domain ID, vendor mapping file,
/// namespace prefix, and auto-discovery toggle.
class DDSConfiguration : public LinkConfiguration
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(int      domainId         READ domainId        WRITE setDomainId        NOTIFY domainIdChanged)
    Q_PROPERTY(QString  vendorMapping    READ vendorMapping   WRITE setVendorMapping   NOTIFY vendorMappingChanged)
    Q_PROPERTY(QString  namespacePrefix  READ namespacePrefix WRITE setNamespacePrefix NOTIFY namespacePrefixChanged)
    Q_PROPERTY(bool     autoDiscover     READ autoDiscover    WRITE setAutoDiscover    NOTIFY autoDiscoverChanged)
    Q_PROPERTY(QString  idlVersion       READ idlVersion      WRITE setIdlVersion      NOTIFY idlVersionChanged)
    Q_PROPERTY(bool     skydroidJoystick READ skydroidJoystick WRITE setSkydroidJoystick NOTIFY skydroidJoystickChanged)
    Q_PROPERTY(QString  peerAddress      READ peerAddress     WRITE setPeerAddress     NOTIFY peerAddressChanged)

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

    QString idlVersion() const { return _idlVersion; }
    void setIdlVersion(const QString &version);

    bool skydroidJoystick() const { return _skydroidJoystick; }
    void setSkydroidJoystick(bool enabled);

    QString peerAddress() const { return _peerAddress; }
    void setPeerAddress(const QString &addr);

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
    void idlVersionChanged();
    void skydroidJoystickChanged();
    void peerAddressChanged();

private:
    int     _domainId        = 0;
    QString _vendorMapping;
    QString _namespacePrefix;
    bool    _autoDiscover    = true;
    QString _idlVersion;
    bool    _skydroidJoystick = false;
    QString _peerAddress;
};

#endif // QGC_ENABLE_DDS
