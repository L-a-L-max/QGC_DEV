#ifdef QGC_ENABLE_ZENOH

#include "ZenohConfiguration.h"

#include <QtCore/QSettings>

static const char *kLocatorKey        = "locator";
static const char *kModeKey           = "mode";
static const char *kVendorMappingKey  = "vendorMapping";
static const char *kNamespacePrefixKey = "namespacePrefix";

ZenohConfiguration::ZenohConfiguration(const QString &name, QObject *parent)
    : LinkConfiguration(name, parent)
{
}

ZenohConfiguration::ZenohConfiguration(const ZenohConfiguration *copy, QObject *parent)
    : LinkConfiguration(copy, parent)
    , _locator(copy->_locator)
    , _mode(copy->_mode)
    , _vendorMapping(copy->_vendorMapping)
    , _namespacePrefix(copy->_namespacePrefix)
{
}

ZenohConfiguration::~ZenohConfiguration() = default;

void ZenohConfiguration::setLocator(const QString &loc)
{
    if (_locator != loc) {
        _locator = loc;
        emit locatorChanged();
    }
}

void ZenohConfiguration::setMode(const QString &m)
{
    if (_mode != m) {
        _mode = m;
        emit modeChanged();
    }
}

void ZenohConfiguration::setVendorMapping(const QString &mapping)
{
    if (_vendorMapping != mapping) {
        _vendorMapping = mapping;
        emit vendorMappingChanged();
    }
}

void ZenohConfiguration::setNamespacePrefix(const QString &ns)
{
    if (_namespacePrefix != ns) {
        _namespacePrefix = ns;
        emit namespacePrefixChanged();
    }
}

void ZenohConfiguration::copyFrom(const LinkConfiguration *source)
{
    LinkConfiguration::copyFrom(source);
    const auto *zenohSource = qobject_cast<const ZenohConfiguration *>(source);
    if (zenohSource) {
        setLocator(zenohSource->locator());
        setMode(zenohSource->mode());
        setVendorMapping(zenohSource->vendorMapping());
        setNamespacePrefix(zenohSource->namespacePrefix());
    }
}

void ZenohConfiguration::loadSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);
    _locator         = settings.value(kLocatorKey).toString();
    _mode            = settings.value(kModeKey, QStringLiteral("peer")).toString();
    _vendorMapping   = settings.value(kVendorMappingKey).toString();
    _namespacePrefix = settings.value(kNamespacePrefixKey).toString();
    settings.endGroup();
}

void ZenohConfiguration::saveSettings(QSettings &settings, const QString &root) const
{
    settings.beginGroup(root);
    settings.setValue(kLocatorKey, _locator);
    settings.setValue(kModeKey, _mode);
    settings.setValue(kVendorMappingKey, _vendorMapping);
    settings.setValue(kNamespacePrefixKey, _namespacePrefix);
    settings.endGroup();
}

#endif // QGC_ENABLE_ZENOH
