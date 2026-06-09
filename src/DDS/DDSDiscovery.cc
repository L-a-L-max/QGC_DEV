#ifdef QGC_ENABLE_DDS

#include "DDSDiscovery.h"
#include "DDSLink.h"

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

#include <dds/dds.h>

Q_LOGGING_CATEGORY(DDSDiscoveryLog, "DDSDiscoveryLog")

DDSDiscovery::DDSDiscovery(QObject *parent)
    : QObject(parent)
{
    connect(&_pollTimer, &QTimer::timeout, this, &DDSDiscovery::_pollBuiltinTopics);
}

DDSDiscovery::~DDSDiscovery()
{
    destroyParticipant();
}

void DDSDiscovery::startDiscovery(int domainId)
{
    if (_running) {
        stopDiscovery();
    }

    // Use the domain-wide shared participant (same as DDSLink).
    // This avoids the CycloneDDS multi-participant data-delivery bug.
    _participant = DDSLink::acquireSharedParticipant(domainId);
    if (_participant <= 0) {
        const QString err = QStringLiteral("Failed to create discovery participant on domain %1").arg(domainId);
        qCWarning(DDSDiscoveryLog) << err;
        emit discoveryError(err);
        return;
    }

    _domainId = domainId;
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

    // Do NOT release the shared participant here — DDSLink may still be
    // using it.  Cleanup happens in destroyParticipant() or ~DDSDiscovery().

    if (_running) {
        _running = false;
        emit runningChanged();
        qInfo() << "[DDSDiscovery] Stopped discovery (participant preserved)";
    }
}

void DDSDiscovery::destroyParticipant()
{
    _pollTimer.stop();
    if (_participant > 0) {
        DDSLink::releaseSharedParticipant(_domainId);
        _participant = DDS_ENTITY_NIL;
    }
}

dds_entity_t DDSDiscovery::releaseParticipant()
{
    _pollTimer.stop();

    // Return the shared participant entity but do NOT release our reference.
    // The caller (DDSLink) will acquire its own reference via
    // acquireSharedParticipant(), and we release ours in destroyParticipant()
    // or the destructor.
    const dds_entity_t p = _participant;

    if (_running) {
        _running = false;
        emit runningChanged();
        qInfo() << "[DDSDiscovery] Released participant (domain" << _domainId << ")";
    }
    return p;
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

    nsSet.remove(QString());  // exclude empty namespace from list
    QStringList result = nsSet.values();
    result.sort();
    return result;
}

#endif // QGC_ENABLE_DDS
