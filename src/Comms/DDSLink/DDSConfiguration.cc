#ifdef QGC_ENABLE_DDS

#include "DDSConfiguration.h"
#include "DDSDiscovery.h"

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
{
}

DDSConfiguration::~DDSConfiguration()
{
    stopDiscovery();
}

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

void DDSConfiguration::startDiscovery()
{
    if (_discovering) {
        return;
    }

    if (!_discovery) {
        _discovery = new DDSDiscovery(this);
        connect(_discovery, &DDSDiscovery::namespacesUpdated,
                this, &DDSConfiguration::_onNamespacesUpdated);
    }

    _discovering = true;
    emit discoveringChanged();

    _discovery->startDiscovery(_domainId);
}

void DDSConfiguration::stopDiscovery()
{
    if (_discovery) {
        _discovery->stopDiscovery();
    }
    if (_discovering) {
        _discovering = false;
        emit discoveringChanged();
    }
}


void DDSConfiguration::_onNamespacesUpdated(const QStringList &namespaces)
{
    if (_discoveredNamespaces != namespaces) {
        _discoveredNamespaces = namespaces;
        emit discoveredNamespacesChanged();
    }
}

void DDSConfiguration::copyFrom(const LinkConfiguration *source)
{
    LinkConfiguration::copyFrom(source);
    auto *ddsSource = const_cast<DDSConfiguration *>(
        qobject_cast<const DDSConfiguration *>(source));
    if (ddsSource) {
        setDomainId(ddsSource->domainId());
        setVendorMapping(ddsSource->vendorMapping());
        setNamespacePrefix(ddsSource->namespacePrefix());
        setAutoDiscover(ddsSource->autoDiscover());
    }
}

void DDSConfiguration::loadSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);
    setDomainId(settings.value(QStringLiteral("domainId"), 0).toInt());
    setVendorMapping(settings.value(QStringLiteral("vendorMapping")).toString());
    setNamespacePrefix(settings.value(QStringLiteral("namespacePrefix")).toString());
    setAutoDiscover(settings.value(QStringLiteral("autoDiscover"), true).toBool());
    settings.endGroup();
}

void DDSConfiguration::saveSettings(QSettings &settings, const QString &root) const
{
    settings.beginGroup(root);
    settings.setValue(QStringLiteral("domainId"), _domainId);
    settings.setValue(QStringLiteral("vendorMapping"), _vendorMapping);
    settings.setValue(QStringLiteral("namespacePrefix"), _namespacePrefix);
    settings.setValue(QStringLiteral("autoDiscover"), _autoDiscover);
    settings.endGroup();
}

#endif // QGC_ENABLE_DDS
