#pragma once

#ifdef QGC_ENABLE_ZENOH

#include "LinkConfiguration.h"

#include <QtQmlIntegration/QtQmlIntegration>

/// Configuration for a Zenoh communication link.
/// Holds user-editable settings: Zenoh locator, mode, vendor mapping,
/// namespace prefix, and auto-discovery toggle.
class ZenohConfiguration : public LinkConfiguration
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(QString  locator          READ locator         WRITE setLocator         NOTIFY locatorChanged)
    Q_PROPERTY(QString  mode             READ mode            WRITE setMode            NOTIFY modeChanged)
    Q_PROPERTY(QString  vendorMapping    READ vendorMapping   WRITE setVendorMapping   NOTIFY vendorMappingChanged)
    Q_PROPERTY(QString  namespacePrefix  READ namespacePrefix WRITE setNamespacePrefix NOTIFY namespacePrefixChanged)

public:
    explicit ZenohConfiguration(const QString &name, QObject *parent = nullptr);
    ZenohConfiguration(const ZenohConfiguration *copy, QObject *parent = nullptr);
    ~ZenohConfiguration() override;

    /// Zenoh locator URI (e.g. "tcp/192.168.1.1:7447")
    QString locator() const { return _locator; }
    void setLocator(const QString &loc);

    /// Zenoh session mode: "peer" or "client"
    QString mode() const { return _mode; }
    void setMode(const QString &m);

    /// Vendor mapping name (e.g. "_default", "cuav_x7pro")
    QString vendorMapping() const { return _vendorMapping; }
    void setVendorMapping(const QString &mapping);

    /// DDS/Zenoh key-expression namespace prefix
    QString namespacePrefix() const { return _namespacePrefix; }
    void setNamespacePrefix(const QString &ns);

    // LinkConfiguration overrides
    LinkType type() const override { return TypeZenoh; }
    void copyFrom(const LinkConfiguration *source) override;
    void loadSettings(QSettings &settings, const QString &root) override;
    void saveSettings(QSettings &settings, const QString &root) const override;
    QString settingsURL() const override { return QStringLiteral("ZenohSettings.qml"); }
    QString settingsTitle() const override { return tr("Zenoh Link Settings"); }

signals:
    void locatorChanged();
    void modeChanged();
    void vendorMappingChanged();
    void namespacePrefixChanged();

private:
    QString _locator;
    QString _mode = QStringLiteral("peer");
    QString _vendorMapping;
    QString _namespacePrefix;
};

#endif // QGC_ENABLE_ZENOH
