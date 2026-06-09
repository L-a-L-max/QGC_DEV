#ifdef QGC_ENABLE_DDS

#include "DDSVehicleManager.h"
#include "DDSLink.h"
#include "DDSConfiguration.h"
#include "DDSDataInjector.h"
#include "LinkInterface.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"
#include "FirmwarePlugin/PX4/px4_custom_mode.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QAtomicInt>

#include <mavlink.h>

DDSVehicleManager::DDSVehicleManager(DDSLink *link, QObject *parent)
    : QObject(parent)
    , _link(link)
{
    (void) connect(link, &DDSLink::ddsMessageReceived,
                   this, &DDSVehicleManager::onDDSMessage);
    (void) connect(&_heartbeatTimer, &QTimer::timeout,
                   this, &DDSVehicleManager::_emitSyntheticHeartbeat);
    (void) connect(link, &LinkInterface::disconnected, this, [this]() {
        _heartbeatTimer.stop();
        _vehicleCreated = false;
    });
}

DDSVehicleManager::~DDSVehicleManager() = default;

void DDSVehicleManager::onDDSMessage(const QString &topicName,
                                     const QHash<QString, QVariant> &fields,
                                     quint64 timestampUs)
{
    Q_UNUSED(timestampUs);

    if (_vehicleCreated) {
        return;
    }

    // Wait for vehicle_status to determine vehicle type
    if (!topicName.contains(QStringLiteral("vehicle_status"))) {
        return;
    }

    int vehicleType = fields.value(QStringLiteral("vehicle_type"), -1).toInt();
    if (vehicleType < 0) {
        return;
    }

    _createVehicle(vehicleType);
}

void DDSVehicleManager::_createVehicle(int vehicleType)
{
    if (_vehicleCreated || !_link) {
        return;
    }

    _vehicleCreated = true;  // Prevent re-entry

    // Map PX4 vehicle_type to MAV_TYPE
    // PX4 vehicle_type values: 0=unknown, 1=rotary_wing, 2=fixed_wing, 3=rover,
    //                          4=airship, 5=boat, 20=vtol
    int mavType;
    switch (vehicleType) {
    case 1:  mavType = MAV_TYPE_QUADROTOR;      break;
    case 2:  mavType = MAV_TYPE_FIXED_WING;     break;
    case 3:  mavType = MAV_TYPE_GROUND_ROVER;   break;
    case 5:  mavType = MAV_TYPE_SURFACE_BOAT;   break;
    case 20: mavType = MAV_TYPE_VTOL_TAILSITTER_DUOROTOR; break;
    default: mavType = MAV_TYPE_QUADROTOR;      break;
    }

    // Check if a Vehicle with the same namespace already exists (reconnect case)
    auto *ddsConfig = qobject_cast<DDSConfiguration *>(_link->linkConfiguration().get());
    const QString nsPrefix = ddsConfig ? ddsConfig->namespacePrefix() : QString();
    Vehicle *existing = _findVehicleByNamespace(nsPrefix);
    if (existing) {
        _vehicleId = existing->id();
        _mavType = mavType;
        qInfo() << "[DDSVehicleManager] Reusing existing vehicle" << _vehicleId
                << "for namespace" << nsPrefix;
        _attachToVehicle(existing, nsPrefix);
        return;
    }

    const int vehicleId = _nextVehicleId();
    constexpr int componentId = MAV_COMP_ID_AUTOPILOT1;

    _vehicleId = vehicleId;
    _mavType = mavType;

    qInfo() << "[DDSVehicleManager] Creating DDS vehicle: id=" << vehicleId
             << "type=" << vehicleType << "MAV_TYPE=" << mavType;

    // Emit the standard heartbeat signal to trigger vehicle creation through
    // the normal MultiVehicleManager flow. This ensures all FactGroups,
    // FirmwarePlugin, parameters etc. are set up correctly.
    emit MAVLinkProtocol::instance()->vehicleHeartbeatInfo(
        _link, vehicleId, componentId, MAV_AUTOPILOT_PX4, mavType);

    // Defer vehicle attachment to let MultiVehicleManager process the heartbeat
    QTimer::singleShot(100, this, [this, vehicleId, nsPrefix]() {
        MultiVehicleManager *mgr = MultiVehicleManager::instance();
        if (!mgr) {
            qWarning() << "[DDSVehicleManager] MultiVehicleManager not available";
            _vehicleCreated = false;
            return;
        }

        Vehicle *vehicle = mgr->getVehicleById(vehicleId);
        if (vehicle) {
            _attachToVehicle(vehicle, nsPrefix);
        } else {
            qWarning() << "[DDSVehicleManager] Vehicle" << vehicleId << "not found after creation";
            _vehicleCreated = false;
        }
    });
}

void DDSVehicleManager::_emitSyntheticHeartbeat()
{
    if (!_link || !_vehicleCreated) {
        _heartbeatTimer.stop();
        return;
    }

    DDSDataInjector *injector = _link->dataInjector();

    // Build base_mode: always custom mode enabled, add armed flag
    uint8_t baseMode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
    if (injector->armingState() == 2) {
        baseMode |= MAV_MODE_FLAG_SAFETY_ARMED;
    }

    // Convert PX4 nav_state to custom_mode for flight mode display
    uint32_t customMode = _navStateToCustomMode(injector->navState());

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

    emit MAVLinkProtocol::instance()->messageReceived(_link, msg);
}

Vehicle *DDSVehicleManager::_findVehicleByNamespace(const QString &ns) const
{
    if (ns.isEmpty()) {
        return nullptr;
    }
    MultiVehicleManager *mgr = MultiVehicleManager::instance();
    if (!mgr) {
        return nullptr;
    }
    QmlObjectListModel *vehicles = mgr->vehicles();
    for (int i = 0; i < vehicles->count(); ++i) {
        Vehicle *v = qobject_cast<Vehicle *>((*vehicles)[i]);
        if (v && v->customName() == ns) {
            return v;
        }
    }
    return nullptr;
}

void DDSVehicleManager::_attachToVehicle(Vehicle *vehicle, const QString &ns)
{
    _link->dataInjector()->setVehicle(vehicle);
    qInfo() << "[DDSVehicleManager] Attached DDSDataInjector to vehicle" << vehicle->id();

    DDSCommandPublisher *pub = _link->commandPublisher();
    vehicle->setDDSCommandPublisher(pub);
    qInfo() << "[DDSVehicleManager] Attached DDSCommandPublisher to vehicle" << vehicle->id()
            << "topic=" << (pub ? pub->topicName() : "null")
            << "matched=" << (pub ? pub->matchedSubscriptionCount() : -1);

    if (!ns.isEmpty()) {
        vehicle->setCustomName(ns);
        qInfo() << "[DDSVehicleManager] Vehicle" << vehicle->id()
                << "display name:" << ns;
    }

    DDSDataInjector *injector = _link->dataInjector();
    connect(injector, &DDSDataInjector::commandAckReceived,
            vehicle, [vehicle](uint32_t command, uint8_t result, uint8_t targetSystem) {
        Q_UNUSED(targetSystem);
        emit vehicle->mavCommandResult(
            vehicle->id(),
            0,
            static_cast<int>(command),
            static_cast<int>(result),
            0);
    });

    _heartbeatTimer.start(1000);
}

int DDSVehicleManager::_nextVehicleId()
{
    static QAtomicInt sCounter(1);
    return sCounter.fetchAndAddRelaxed(1);
}

uint32_t DDSVehicleManager::_navStateToCustomMode(int navState)
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

#endif // QGC_ENABLE_DDS
