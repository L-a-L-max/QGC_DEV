#ifdef QGC_ENABLE_DDS

#include "DDSVehicleManager.h"
#include "DDSLink.h"
#include "DDSDataInjector.h"
#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>

DDSVehicleManager::DDSVehicleManager(DDSLink *link, QObject *parent)
    : QObject(parent)
    , _link(link)
{
    (void) connect(link, &DDSLink::ddsMessageReceived,
                   this, &DDSVehicleManager::onDDSMessage);
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

    // Use vehicle ID 1 for the DDS vehicle (standard PX4 SITL ID)
    constexpr int vehicleId = 1;
    constexpr int componentId = MAV_COMP_ID_AUTOPILOT1;

    qInfo() << "[DDSVehicleManager] Creating DDS vehicle: id=" << vehicleId
             << "type=" << vehicleType << "MAV_TYPE=" << mavType;

    // Emit the standard heartbeat signal to trigger vehicle creation through
    // the normal MultiVehicleManager flow. This ensures all FactGroups,
    // FirmwarePlugin, parameters etc. are set up correctly.
    emit MAVLinkProtocol::instance()->vehicleHeartbeatInfo(
        _link, vehicleId, componentId, MAV_AUTOPILOT_PX4, mavType);

    // Defer vehicle attachment to let MultiVehicleManager process the heartbeat
    QTimer::singleShot(100, this, [this, vehicleId]() {
        MultiVehicleManager *mgr = MultiVehicleManager::instance();
        if (!mgr) {
            qWarning() << "[DDSVehicleManager] MultiVehicleManager not available";
            _vehicleCreated = false;
            return;
        }

        Vehicle *vehicle = mgr->getVehicleById(vehicleId);
        if (vehicle) {
            _link->dataInjector()->setVehicle(vehicle);
            qInfo() << "[DDSVehicleManager] Attached DDSDataInjector to vehicle" << vehicleId;
        } else {
            qWarning() << "[DDSVehicleManager] Vehicle" << vehicleId << "not found after creation";
            _vehicleCreated = false;
        }
    });
}

#endif // QGC_ENABLE_DDS
