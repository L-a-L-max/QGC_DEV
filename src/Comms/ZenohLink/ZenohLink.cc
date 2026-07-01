#ifdef QGC_ENABLE_ZENOH

#include "ZenohLink.h"
#include "QGCLoggingCategory.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

#include <QtCore/QDateTime>
#include <mavlink.h>
#include <cstring>

#include "FirmwarePlugin/PX4/px4_custom_mode.h"

QGC_LOGGING_CATEGORY_ON(ZenohLinkLog, "Comms.ZenohLink")

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

ZenohLink::ZenohLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
    , _dataInjector(&_mappingEngine, &_transformRegistry, this)
{
    _pollTimer.setInterval(10);
    (void)QObject::connect(&_pollTimer, &QTimer::timeout, this, &ZenohLink::_onPollTimer);
    (void)QObject::connect(this, &ZenohLink::zenohMessageReceived,
                           &_dataInjector, &DDSDataInjector::onDDSMessage);
    (void)QObject::connect(this, &ZenohLink::zenohMessageReceived,
                           this, &ZenohLink::_onZenohMessage);
    (void)QObject::connect(&_heartbeatTimer, &QTimer::timeout,
                           this, &ZenohLink::_emitSyntheticHeartbeat);
    (void)QObject::connect(this, &LinkInterface::disconnected,
                           &_heartbeatTimer, &QTimer::stop);
}

ZenohLink::~ZenohLink()
{
    disconnect();
}

// ---------------------------------------------------------------------------
// Configuration helper
// ---------------------------------------------------------------------------

ZenohConfiguration *ZenohLink::_zenohConfig() const
{
    return qobject_cast<ZenohConfiguration *>(_config.get());
}

// ---------------------------------------------------------------------------
// Connect / Disconnect
// ---------------------------------------------------------------------------

bool ZenohLink::_connect()
{
    if (_connected) {
        return true;
    }

    const ZenohConfiguration *config = _zenohConfig();
    if (!config) {
        emit communicationError(tr("Zenoh Link"), tr("Invalid Zenoh configuration"));
        return false;
    }

    const QString mappingName = config->vendorMapping().isEmpty()
                                    ? QStringLiteral("_default")
                                    : config->vendorMapping();
    if (!_mappingEngine.loadMapping(mappingName)) {
        qCWarning(ZenohLinkLog) << "Failed to load mapping:" << mappingName;
        emit communicationError(tr("Zenoh Link"),
                                tr("Failed to load mapping table: %1").arg(mappingName));
        return false;
    }

    qInfo() << "[ZenohLink] Loaded mapping:" << mappingName
            << "topics:" << _mappingEngine.topicCount()
            << "fields:" << _mappingEngine.fieldCount();

    if (!_openSession()) {
        emit communicationError(tr("Zenoh Link"), tr("Failed to open Zenoh session"));
        return false;
    }

    _subscribeToTopics(_mappingEngine.allTopicNames());

    _pollTimer.start();
    _connected = true;

    qInfo() << "[ZenohLink] Connected";
    emit connected();
    return true;
}

void ZenohLink::disconnect()
{
    if (!_connected) {
        return;
    }

    _pollTimer.stop();
    _heartbeatTimer.stop();
    _closeSession();
    _connected = false;

    emit disconnected();
    qInfo() << "[ZenohLink] Disconnected";
}

void ZenohLink::_writeBytes(const QByteArray &bytes)
{
    Q_UNUSED(bytes);
}

// ---------------------------------------------------------------------------
// Zenoh Session Management
// ---------------------------------------------------------------------------

bool ZenohLink::_openSession()
{
    const ZenohConfiguration *config = _zenohConfig();

    z_owned_config_t zconfig;
    z_config_default(&zconfig);

    const QByteArray modeBytes = config->mode().toUtf8();
    zp_config_insert(z_loan_mut(zconfig), Z_CONFIG_MODE_KEY, modeBytes.constData());

    if (!config->locator().isEmpty()) {
        const QByteArray locBytes = config->locator().toUtf8();
        zp_config_insert(z_loan_mut(zconfig), Z_CONFIG_CONNECT_KEY, locBytes.constData());
    }

    qInfo() << "[ZenohLink] Opening session — mode:" << config->mode()
            << "locator:" << config->locator();

    if (z_open(&_session, z_move(zconfig), NULL) < 0) {
        qCWarning(ZenohLinkLog) << "Failed to open Zenoh session";
        return false;
    }

    if (zp_start_read_task(z_loan_mut(_session), NULL) < 0) {
        qCWarning(ZenohLinkLog) << "Failed to start Zenoh read task";
        z_drop(z_move(_session));
        return false;
    }
    if (zp_start_lease_task(z_loan_mut(_session), NULL) < 0) {
        qCWarning(ZenohLinkLog) << "Failed to start Zenoh lease task";
        zp_stop_read_task(z_loan_mut(_session));
        z_drop(z_move(_session));
        return false;
    }

    _sessionOpen = true;
    qInfo() << "[ZenohLink] Session opened";
    return true;
}

void ZenohLink::_closeSession()
{
    for (auto it = _subscribers.begin(); it != _subscribers.end(); ++it) {
        z_drop(z_move(it.value().subscriber));
    }
    _subscribers.clear();

    qDeleteAll(_callbackContexts);
    _callbackContexts.clear();
    _receivedTopics.clear();

    if (_sessionOpen) {
        zp_stop_read_task(z_loan_mut(_session));
        zp_stop_lease_task(z_loan_mut(_session));
        z_drop(z_move(_session));
        _sessionOpen = false;
    }
}

// ---------------------------------------------------------------------------
// Zenoh Subscription
// ---------------------------------------------------------------------------

void ZenohLink::_zenohDataCallback(z_loaned_sample_t *sample, void *ctx)
{
    auto *cbCtx = static_cast<CallbackContext *>(ctx);
    if (!cbCtx || !cbCtx->link) {
        return;
    }

    z_owned_bytes_t payloadOwned;
    z_bytes_clone(&payloadOwned, z_sample_payload(sample));

    z_owned_slice_t slice;
    z_bytes_to_slice(z_loan(payloadOwned), &slice);

    const uint8_t *data = z_slice_data(z_loan(slice));
    const size_t len = z_slice_len(z_loan(slice));

    if (data && len > 0) {
        QHash<QString, QVariant> fields = cbCtx->link->_parseCdrPayload(
            cbCtx->topicName, data, len);

        if (!fields.isEmpty()) {
            QMutexLocker lock(&cbCtx->link->_pendingMutex);
            cbCtx->link->_pendingSamples.append({cbCtx->topicName, fields});
        }
    }

    z_drop(z_move(slice));
    z_drop(z_move(payloadOwned));
}

QHash<QString, QVariant> ZenohLink::_parseCdrPayload(const QString &topicName,
                                                     const uint8_t *data, size_t len)
{
    const DDSTopicMapping *mapping = _mappingEngine.topicMapping(topicName);
    if (!mapping) {
        return {};
    }

    const DDSTypeEntry *typeEntry = mapping->ddsTypeName.isEmpty()
        ? nullptr
        : _typeRegistry.typeEntry(mapping->ddsTypeName);

    if (!typeEntry || !typeEntry->extractor) {
        return {};
    }

    // CDR payload from zenoh-bridge-dds: 4-byte header (byte 0 = endianness)
    // followed by the struct data in the same layout as the C struct on
    // little-endian (ARM64/x86_64).
    constexpr size_t kCdrHeaderSize = 4;
    if (len <= kCdrHeaderSize) {
        return {};
    }

    const uint8_t *structData = data + kCdrHeaderSize;
    return typeEntry->extractor(structData);
}

void ZenohLink::_subscribeToTopics(const QStringList &topicNames)
{
    const ZenohConfiguration *config = _zenohConfig();
    const QString prefix = config ? config->namespacePrefix() : QString();
    const QString rtPrefix = prefix.isEmpty() ? QStringLiteral("rt/") : prefix;

    int subscribedCount = 0;
    int stubCount = 0;

    for (const QString &topicName : topicNames) {
        const DDSTopicMapping *mapping = _mappingEngine.topicMapping(topicName);
        const QString typeName = mapping ? mapping->ddsTypeName : QString();
        const DDSTypeEntry *typeEntry = typeName.isEmpty()
            ? nullptr
            : _typeRegistry.typeEntry(typeName);

        if (!typeEntry || !typeEntry->extractor) {
            stubCount++;
            continue;
        }

        const QString keyExpr = rtPrefix + topicName;
        const QByteArray keyBytes = keyExpr.toUtf8();

        z_view_keyexpr_t ke;
        if (z_view_keyexpr_from_str(&ke, keyBytes.constData()) < 0) {
            qCWarning(ZenohLinkLog) << "Invalid key expression:" << keyExpr;
            stubCount++;
            continue;
        }

        auto *cbCtx = new CallbackContext{this, topicName, typeEntry->extractor};
        _callbackContexts.append(cbCtx);

        z_owned_closure_sample_t callback;
        z_closure(&callback, _zenohDataCallback, NULL, cbCtx);

        SubInfo info;
        if (z_declare_subscriber(z_loan(_session), &info.subscriber, z_loan(ke),
                                 z_move(callback), NULL) < 0) {
            qCWarning(ZenohLinkLog) << "Failed to subscribe:" << keyExpr;
            stubCount++;
            continue;
        }

        _subscribers.insert(topicName, info);
        subscribedCount++;
        qCDebug(ZenohLinkLog) << "Subscribed:" << keyExpr;
    }

    qInfo() << "[ZenohLink] Subscribed:" << subscribedCount << "topics,"
            << stubCount << "stubs";
}

// ---------------------------------------------------------------------------
// Poll Timer — drain Zenoh callback thread queue on the main thread
// ---------------------------------------------------------------------------

void ZenohLink::_onPollTimer()
{
    if (!_connected) {
        return;
    }

    const quint64 now = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000;

    QList<PendingSample> samples;
    {
        QMutexLocker lock(&_pendingMutex);
        samples.swap(_pendingSamples);
    }

    for (const PendingSample &sample : samples) {
        if (!_receivedTopics.contains(sample.topicName)) {
            _receivedTopics.insert(sample.topicName);
            qInfo() << "[ZenohLink] First data:" << sample.topicName
                    << "fields:" << sample.fields.size();
        }
        emit zenohMessageReceived(sample.topicName, sample.fields, now);
    }
}

// ---------------------------------------------------------------------------
// Zenoh Publishing
// ---------------------------------------------------------------------------

bool ZenohLink::publishCdr(const QString &keyExpr, const void *data, size_t len)
{
    if (!_sessionOpen) {
        return false;
    }

    const QByteArray keyBytes = keyExpr.toUtf8();
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, keyBytes.constData()) < 0) {
        return false;
    }

    // 4-byte CDR header: little-endian + 3 zero-bytes, then struct data
    const size_t totalLen = 4 + len;
    QByteArray cdrBuf(static_cast<int>(totalLen), '\0');
    cdrBuf[0] = 0x01;
    memcpy(cdrBuf.data() + 4, data, len);

    z_owned_bytes_t payload;
    z_bytes_copy_from_buf(&payload,
                          reinterpret_cast<const uint8_t *>(cdrBuf.constData()),
                          static_cast<size_t>(cdrBuf.size()));

    z_put_options_t opts;
    z_put_options_default(&opts);

    const z_result_t rc = z_put(z_loan(_session), z_loan(ke), z_move(payload), &opts);
    return rc == 0;
}

// ---------------------------------------------------------------------------
// Vehicle Creation (mirrors DDSVehicleManager logic)
// ---------------------------------------------------------------------------

void ZenohLink::_onZenohMessage(const QString &topicName,
                                const QHash<QString, QVariant> &fields,
                                quint64 timestampUs)
{
    Q_UNUSED(timestampUs);

    if (_vehicleCreated) {
        return;
    }

    if (!topicName.contains(QStringLiteral("vehicle_status"))) {
        return;
    }

    _tryCreateVehicle(fields);
}

void ZenohLink::_tryCreateVehicle(const QHash<QString, QVariant> &fields)
{
    if (_vehicleCreated) {
        return;
    }

    const int vehicleType = fields.value(QStringLiteral("vehicle_type"), -1).toInt();
    if (vehicleType < 0) {
        return;
    }

    _vehicleCreated = true;

    int mavType;
    switch (vehicleType) {
    case 1:  mavType = MAV_TYPE_QUADROTOR;      break;
    case 2:  mavType = MAV_TYPE_FIXED_WING;     break;
    case 3:  mavType = MAV_TYPE_GROUND_ROVER;   break;
    case 5:  mavType = MAV_TYPE_SURFACE_BOAT;   break;
    case 20: mavType = MAV_TYPE_VTOL_TAILSITTER_DUOROTOR; break;
    default: mavType = MAV_TYPE_QUADROTOR;      break;
    }

    constexpr int vehicleId = 1;
    constexpr int componentId = MAV_COMP_ID_AUTOPILOT1;

    _vehicleId = vehicleId;
    _mavType = mavType;

    qInfo() << "[ZenohLink] Creating vehicle — id:" << vehicleId
            << "type:" << vehicleType << "MAV_TYPE:" << mavType;

    emit MAVLinkProtocol::instance()->vehicleHeartbeatInfo(
        this, vehicleId, componentId, MAV_AUTOPILOT_PX4, mavType);

    QTimer::singleShot(100, this, [this]() {
        MultiVehicleManager *mgr = MultiVehicleManager::instance();
        if (!mgr) {
            qCWarning(ZenohLinkLog) << "MultiVehicleManager not available";
            _vehicleCreated = false;
            return;
        }

        Vehicle *vehicle = mgr->getVehicleById(_vehicleId);
        if (vehicle) {
            _dataInjector.setVehicle(vehicle);
            qInfo() << "[ZenohLink] Attached DDSDataInjector to vehicle" << _vehicleId;

            _heartbeatTimer.start(1000);
        } else {
            qCWarning(ZenohLinkLog) << "Vehicle" << _vehicleId << "not found after creation";
            _vehicleCreated = false;
        }
    });
}

void ZenohLink::_emitSyntheticHeartbeat()
{
    if (!_connected || !_vehicleCreated) {
        _heartbeatTimer.stop();
        return;
    }

    uint8_t baseMode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
    if (_dataInjector.armingState() == 2) {
        baseMode |= MAV_MODE_FLAG_SAFETY_ARMED;
    }

    const uint32_t customMode = _navStateToCustomMode(_dataInjector.navState());

    mavlink_message_t msg{};
    mavlink_msg_heartbeat_pack_chan(
        static_cast<uint8_t>(_vehicleId),
        MAV_COMP_ID_AUTOPILOT1,
        0,
        &msg,
        static_cast<uint8_t>(_mavType),
        MAV_AUTOPILOT_PX4,
        baseMode,
        customMode,
        MAV_STATE_ACTIVE);

    emit MAVLinkProtocol::instance()->messageReceived(this, msg);
}

uint32_t ZenohLink::_navStateToCustomMode(int navState)
{
    union px4_custom_mode cm{};
    cm.data = 0;

    switch (navState) {
    case 0:  cm.main_mode = PX4_CUSTOM_MAIN_MODE_MANUAL;     break;
    case 1:  cm.main_mode = PX4_CUSTOM_MAIN_MODE_ALTCTL;     break;
    case 2:  cm.main_mode = PX4_CUSTOM_MAIN_MODE_POSCTL;     break;
    case 3:  cm.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
             cm.sub_mode  = PX4_CUSTOM_SUB_MODE_AUTO_MISSION; break;
    case 4:  cm.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
             cm.sub_mode  = PX4_CUSTOM_SUB_MODE_AUTO_LOITER;  break;
    case 5:  cm.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
             cm.sub_mode  = PX4_CUSTOM_SUB_MODE_AUTO_RTL;     break;
    case 13: cm.main_mode = PX4_CUSTOM_MAIN_MODE_ACRO;       break;
    case 14: cm.main_mode = PX4_CUSTOM_MAIN_MODE_OFFBOARD;   break;
    case 15: cm.main_mode = PX4_CUSTOM_MAIN_MODE_STABILIZED; break;
    case 17: cm.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
             cm.sub_mode  = PX4_CUSTOM_SUB_MODE_AUTO_TAKEOFF; break;
    case 18: cm.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
             cm.sub_mode  = PX4_CUSTOM_SUB_MODE_AUTO_LAND;    break;
    default: cm.main_mode = PX4_CUSTOM_MAIN_MODE_POSCTL;     break;
    }

    return cm.data;
}

#endif // QGC_ENABLE_ZENOH
