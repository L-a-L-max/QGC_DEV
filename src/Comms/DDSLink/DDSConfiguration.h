#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

/// @file DDSConfiguration.h
/// @brief Configuration for a DDS communication link.
///
/// DDSConfiguration holds user-editable settings for a single DDS link:
///   - DDS domain ID (default 0, matching PX4 XRCE-DDS Agent default)
///   - Vendor mapping file name (selects which JSON mapping table to load)
///   - Namespace prefix (for multi-vehicle DDS namespace isolation)
///   - Auto-discover toggle (use DDS topic discovery to detect vendor topics)
///
/// It inherits from LinkConfiguration (QGC's base class for all link types)
/// and adds TypeDDS to the LinkType enum.

// Forward declaration — the actual base class lives in QGC source tree.
// When integrating into QGC, replace this stub with: #include "LinkConfiguration.h"
#ifndef QGC_LINK_CONFIGURATION_INCLUDED
class LinkConfiguration : public QObject
{
    Q_OBJECT
public:
    enum LinkType {
        TypeSerial = 0,
        TypeUdp,
        TypeTcp,
        TypeBluetooth,
        TypeMock,
        TypeLogReplay,
        TypeDDS,       // <-- NEW
        TypeLast
    };
    Q_ENUM(LinkType)

    explicit LinkConfiguration(const QString &name, QObject *parent = nullptr)
        : QObject(parent), _name(name) {}
    LinkConfiguration(const LinkConfiguration *copy, QObject *parent = nullptr)
        : QObject(parent), _name(copy->_name) {}
    virtual ~LinkConfiguration() = default;

    QString name() const { return _name; }
    virtual LinkType type() const = 0;
    virtual void copyFrom(const LinkConfiguration *source) { _name = source->_name; }
    virtual void loadSettings(QSettings &settings, const QString &root) = 0;
    virtual void saveSettings(QSettings &settings, const QString &root) const = 0;
    virtual QString settingsURL() const { return {}; }
    virtual QString settingsTitle() const { return {}; }

private:
    QString _name;
};
#define QGC_LINK_CONFIGURATION_INCLUDED
#endif

class DDSConfiguration : public LinkConfiguration
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(int      domainId         READ domainId        WRITE setDomainId        NOTIFY domainIdChanged)
    Q_PROPERTY(QString  vendorMapping    READ vendorMapping   WRITE setVendorMapping   NOTIFY vendorMappingChanged)
    Q_PROPERTY(QString  namespacePrefix  READ namespacePrefix WRITE setNamespacePrefix NOTIFY namespacePrefixChanged)
    Q_PROPERTY(bool     autoDiscover     READ autoDiscover    WRITE setAutoDiscover    NOTIFY autoDiscoverChanged)

public:
    explicit DDSConfiguration(const QString &name, QObject *parent = nullptr);
    DDSConfiguration(const DDSConfiguration *copy, QObject *parent = nullptr);
    ~DDSConfiguration() override;

    // --- Accessors ---

    int domainId() const { return _domainId; }
    void setDomainId(int id);

    /// Vendor mapping file name (without path). Empty means use _default.json.
    QString vendorMapping() const { return _vendorMapping; }
    void setVendorMapping(const QString &mapping);

    /// DDS namespace prefix, e.g. "/drone1". Empty for no prefix.
    QString namespacePrefix() const { return _namespacePrefix; }
    void setNamespacePrefix(const QString &ns);

    /// Whether to use DDS topic discovery to auto-detect vendor topics.
    bool autoDiscover() const { return _autoDiscover; }
    void setAutoDiscover(bool enabled);

    // --- LinkConfiguration overrides ---
    LinkType type() const override { return TypeDDS; }
    void copyFrom(const LinkConfiguration *source) override;
    void loadSettings(QSettings &settings, const QString &root) override;
    void saveSettings(QSettings &settings, const QString &root) const override;
    QString settingsURL() const override { return QStringLiteral("qrc:/qml/QGroundControl/Comms/DDSSettings.qml"); }
    QString settingsTitle() const override { return tr("DDS Link Settings"); }

signals:
    void domainIdChanged();
    void vendorMappingChanged();
    void namespacePrefixChanged();
    void autoDiscoverChanged();

private:
    int     _domainId        = 0;       // DDS domain ID (PX4 default: 0)
    QString _vendorMapping;              // empty = _default.json
    QString _namespacePrefix;            // empty = no namespace
    bool    _autoDiscover    = true;     // use topic discovery
};

#endif // QGC_ENABLE_DDS
