#ifdef QGC_ENABLE_DDS

#include "DDSConfiguration.h"

DDSConfiguration::DDSConfiguration(const QString &name, QObject *parent)
    : LinkConfiguration(name, parent)
{
}

DDSConfiguration::DDSConfiguration(const DDSConfiguration *copy, QObject *parent)
    : LinkConfiguration(copy, parent)
    , _domainId(copy->_domainId)
    , _vendorMapping(copy->_vendorMapping)
    , _namespacePrefix(copy->_namespacePrefix)
    , _autoDiscover(copy->_autoDiscover)
    , _idlVersion(copy->_idlVersion)
{
}

DDSConfiguration::~DDSConfiguration() = default;

void DDSConfiguration::setDomainId(int id)
{
    if (_domainId != id) {
        _domainId = id;
        emit domainIdChanged();
    }
}

void DDSConfiguration::setVendorMapping(const QString &mapping)
{
    if (_vendorMapping != mapping) {
        _vendorMapping = mapping;
        emit vendorMappingChanged();
    }
}

void DDSConfiguration::setNamespacePrefix(const QString &ns)
{
    if (_namespacePrefix != ns) {
        _namespacePrefix = ns;
        emit namespacePrefixChanged();
    }
}

void DDSConfiguration::setAutoDiscover(bool enabled)
{
    if (_autoDiscover != enabled) {
        _autoDiscover = enabled;
        emit autoDiscoverChanged();
    }
}

void DDSConfiguration::setIdlVersion(const QString &version)
{
    if (_idlVersion != version) {
        _idlVersion = version;
        emit idlVersionChanged();
    }
}

void DDSConfiguration::copyFrom(const LinkConfiguration *source)
{
    LinkConfiguration::copyFrom(source);
    const auto *ddsSource = qobject_cast<const DDSConfiguration *>(source);
    if (ddsSource) {
        setDomainId(ddsSource->domainId());
        setVendorMapping(ddsSource->vendorMapping());
        setNamespacePrefix(ddsSource->namespacePrefix());
        setAutoDiscover(ddsSource->autoDiscover());
        setIdlVersion(ddsSource->idlVersion());
    }
}

void DDSConfiguration::loadSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);
    setDomainId(settings.value(QStringLiteral("domainId"), 0).toInt());
    setVendorMapping(settings.value(QStringLiteral("vendorMapping")).toString());
    setNamespacePrefix(settings.value(QStringLiteral("namespacePrefix")).toString());
    setAutoDiscover(settings.value(QStringLiteral("autoDiscover"), true).toBool());
    setIdlVersion(settings.value(QStringLiteral("idlVersion")).toString());
    settings.endGroup();
}

void DDSConfiguration::saveSettings(QSettings &settings, const QString &root) const
{
    settings.beginGroup(root);
    settings.setValue(QStringLiteral("domainId"), _domainId);
    settings.setValue(QStringLiteral("vendorMapping"), _vendorMapping);
    settings.setValue(QStringLiteral("namespacePrefix"), _namespacePrefix);
    settings.setValue(QStringLiteral("autoDiscover"), _autoDiscover);
    settings.setValue(QStringLiteral("idlVersion"), _idlVersion);
    settings.endGroup();
}

#endif // QGC_ENABLE_DDS
