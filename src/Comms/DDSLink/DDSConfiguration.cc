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
    if (_pendingParticipant > 0) {
        dds_delete(_pendingParticipant);
        _pendingParticipant = DDS_ENTITY_NIL;
    }
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

dds_entity_t DDSConfiguration::takeDiscoveryParticipant()
{
    // Prefer the participant transferred from the edit-copy during copyFrom()
    if (_pendingParticipant > 0) {
        dds_entity_t p = _pendingParticipant;
        _pendingParticipant = DDS_ENTITY_NIL;
        qInfo() << "[DDSConfiguration] Providing pending participant:" << p;
        return p;
    }
    if (!_discovery) {
        return DDS_ENTITY_NIL;
    }
    dds_entity_t p = _discovery->releaseParticipant();
    if (_discovering) {
        _discovering = false;
        emit discoveringChanged();
    }
    return p;
}

void DDSConfiguration::returnParticipant(dds_entity_t participant)
{
    if (participant > 0) {
        _pendingParticipant = participant;
        qInfo() << "[DDSConfiguration] Saved participant" << participant << "for reuse";
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

        // Transfer the discovery participant so DDSLink can reuse it
        // instead of creating a new one (avoids RTPS re-discovery delay).
        if (ddsSource->_discovery) {
            _pendingParticipant = ddsSource->_discovery->releaseParticipant();
            if (_pendingParticipant > 0) {
                qInfo() << "[DDSConfiguration] Transferred participant"
                        << _pendingParticipant << "from edit config";
            }
        }
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
