#ifdef QGC_ENABLE_DDS

#include "DDSDiscovery.h"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtNetwork/QNetworkInterface>

#include <dds/dds.h>

Q_LOGGING_CATEGORY(DDSDiscoveryLog, "DDSDiscoveryLog")

DDSDiscovery::DDSDiscovery(QObject *parent)
    : QObject(parent)
{
    connect(&_pollTimer, &QTimer::timeout, this, &DDSDiscovery::_pollBuiltinTopics);
}

DDSDiscovery::~DDSDiscovery()
{
    stopDiscovery();
}

void DDSDiscovery::startDiscovery(int domainId)
{
    // Stop any previous scan first
    if (_running) {
        stopDiscovery();
    }

    // Build a minimal CycloneDDS config that enables multicast on all
    // physical interfaces – same logic as DDSLink::_createParticipant().
    if (qEnvironmentVariableIsEmpty("CYCLONEDDS_URI")) {
        QString interfacesXml;
        const auto allIfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &iface : allIfaces) {
            if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
            if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
            const QString name = iface.name();
            if (name.startsWith(QStringLiteral("docker")) ||
                name.startsWith(QStringLiteral("br-")) ||
                name.startsWith(QStringLiteral("veth")) ||
                name.startsWith(QStringLiteral("virbr"))) {
                continue;
            }
            bool hasIpv4 = false;
            for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    hasIpv4 = true;
                    break;
                }
            }
            if (!hasIpv4) continue;
            interfacesXml += QStringLiteral("        <NetworkInterface name=\"%1\" multicast=\"true\"/>\n").arg(name);
        }

        QString config;
        if (interfacesXml.isEmpty()) {
            config = QStringLiteral(
                "<CycloneDDS>"
                "  <Domain id=\"any\">"
                "    <Compatibility><StandardsConformance>lax</StandardsConformance></Compatibility>"
                "  </Domain>"
                "</CycloneDDS>");
        } else {
            config = QStringLiteral(
                "<CycloneDDS>"
                "  <Domain id=\"any\">"
                "    <General><Interfaces>\n%1"
                "    </Interfaces></General>"
                "    <Compatibility><StandardsConformance>lax</StandardsConformance></Compatibility>"
                "  </Domain>"
                "</CycloneDDS>").arg(interfacesXml);
        }
        qputenv("CYCLONEDDS_URI", config.toUtf8());
    }

    _participant = dds_create_participant(static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
    if (_participant < 0) {
        const QString err = QStringLiteral("Failed to create discovery participant on domain %1").arg(domainId);
        qCWarning(DDSDiscoveryLog) << err;
        emit discoveryError(err);
        return;
    }

    _namespaces.clear();
    _running = true;
    emit runningChanged();

    qInfo() << "[DDSDiscovery] Started discovery on domain" << domainId;

    // Poll immediately, then every 2 seconds
    _pollBuiltinTopics();
    _pollTimer.start(2000);
}

void DDSDiscovery::stopDiscovery()
{
    _pollTimer.stop();

    if (_participant > 0) {
        dds_delete(_participant);
        _participant = DDS_ENTITY_NIL;
    }

    if (_running) {
        _running = false;
        emit runningChanged();
        qInfo() << "[DDSDiscovery] Stopped discovery";
    }
}

void DDSDiscovery::_pollBuiltinTopics()
{
    if (_participant <= 0) {
        return;
    }

    const QStringList fresh = _extractNamespaces(_participant);
    if (fresh != _namespaces) {
        _namespaces = fresh;
        qInfo() << "[DDSDiscovery] Namespaces updated:" << _namespaces;
        emit namespacesUpdated(_namespaces);
    }
}

QStringList DDSDiscovery::_extractNamespaces(dds_entity_t participant)
{
    // Read the builtin DCPSPublication topic to discover all known writers
    const dds_entity_t reader = dds_create_reader(
        participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, nullptr, nullptr);
    if (reader < 0) {
        qCWarning(DDSDiscoveryLog) << "Failed to create builtin publication reader";
        return {};
    }

    constexpr int kMaxSamples = 128;
    void *samples[kMaxSamples];
    dds_sample_info_t infos[kMaxSamples];
    memset(samples, 0, sizeof(samples));

    const int n = dds_take(reader, samples, infos, kMaxSamples, kMaxSamples);

    QSet<QString> nsSet;

    // Regex: rt/{namespace}/fmu/  — extract the namespace part
    static const QRegularExpression re(QStringLiteral("^rt/([^/]+)/fmu/"));

    for (int i = 0; i < n; ++i) {
        if (!infos[i].valid_data || !samples[i]) {
            continue;
        }

        const auto *ep = static_cast<const dds_builtintopic_endpoint_t *>(samples[i]);
        const QString topicName = QString::fromUtf8(ep->topic_name);

        const QRegularExpressionMatch match = re.match(topicName);
        if (match.hasMatch()) {
            nsSet.insert(match.captured(1));
        } else if (topicName.startsWith(QStringLiteral("rt/fmu/"))) {
            // No namespace — this is a default PX4 without UXRCE_DDS_NS
            nsSet.insert(QString());
        }
    }

    if (n > 0) {
        dds_return_loan(reader, samples, n);
    }
    dds_delete(reader);

    QStringList result = nsSet.values();
    result.sort();
    return result;
}

#endif // QGC_ENABLE_DDS
