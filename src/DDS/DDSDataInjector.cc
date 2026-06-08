#ifdef QGC_ENABLE_DDS

#include "DDSDataInjector.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "Vehicle.h"
#include "Fact.h"
#include "FactGroup.h"
#include "QmlObjectListModel.h"
#include "BatteryFactGroupListModel.h"
#include "QGCMAVLink.h"
#include "px4_custom_mode.h"

#include <QtCore/QDebug>
#include <QtPositioning/QGeoCoordinate>

DDSDataInjector::DDSDataInjector(DDSMappingEngine *engine,
                                 DDSTransformRegistry *transforms,
                                 QObject *parent)
    : QObject(parent)
    , _mappingEngine(engine)
    , _transformRegistry(transforms)
{
}

DDSDataInjector::~DDSDataInjector() = default;

void DDSDataInjector::setVehicle(Vehicle *vehicle)
{
    _vehicle = vehicle;
    qInfo() << "[DDSDataInjector] Vehicle set:"
            << (vehicle ? "attached" : "detached");
}

void DDSDataInjector::onDDSMessage(const QString &topicName,
                                    const QHash<QString, QVariant> &fields,
                                    quint64 timestampUs)
{
    Q_UNUSED(timestampUs);
    _messagesProcessed++;

    if (!_vehicle) {
        return;
    }

    // Handle special topics that need direct Vehicle property updates
    if (topicName.contains(QLatin1String("vehicle_global_position"))) {
        _updateVehicleCoordinate(fields);
    }
    if (topicName.contains(QLatin1String("vehicle_status"))) {
        _updateVehicleState(fields);
    }
    if (topicName.contains(QLatin1String("home_position"))) {
        _updateHomePosition(fields);
    }
    if (topicName.contains(QLatin1String("land_detected"))) {
        _updateLandDetected(fields);
    }
    if (topicName.contains(QLatin1String("failsafe_flags"))) {
        _updateReadyToFly(fields);
    }
    if (topicName.contains(QLatin1String("vehicle_command_ack"))) {
        _handleCommandAck(fields);
    }
    if (topicName.contains(QLatin1String("battery_status"))) {
        _ensureBatteryExists();
    }
    if (topicName.contains(QLatin1String("vehicle_gps_position"))) {
        FactGroup *gps = _resolveFactGroup(QStringLiteral("gps"));
        if (gps) {
            gps->setTelemetryAvailable(true);
        }
    }

    const DDSTopicMapping *mapping = _mappingEngine->topicMapping(topicName);
    if (!mapping) {
        _unmappedSkipped++;
        return;
    }

    for (const DDSFieldMapping &fieldMapping : mapping->fields) {
        QVariant value;

        if (!fieldMapping.transform.isEmpty()) {
            DDSTransformFunc func = _transformRegistry->transform(fieldMapping.transform);
            if (func) {
                QHash<QString, QVariant> transformInput = fields;
                if (fields.contains(fieldMapping.ddsField)) {
                    transformInput.insert(QStringLiteral("_value"),
                                         fields.value(fieldMapping.ddsField));
                }
                value = func(transformInput);
            } else {
                qWarning() << "[DDSDataInjector] Unknown transform:"
                           << fieldMapping.transform;
                continue;
            }
        } else {
            if (!fields.contains(fieldMapping.ddsField)) {
                continue;
            }
            value = fields.value(fieldMapping.ddsField);
        }

        if (fieldMapping.scale != 1.0 || fieldMapping.offset != 0.0) {
            bool ok = false;
            double numVal = value.toDouble(&ok);
            if (ok) {
                numVal = numVal * fieldMapping.scale + fieldMapping.offset;
                value = QVariant(numVal);
            }
        }

        const QString &factGroup = fieldMapping.factGroup;
        const QString &factName = fieldMapping.factName;
        _injectField(factGroup, factName, value);
    }
}

FactGroup *DDSDataInjector::_resolveFactGroup(const QString &path) const
{
    if (!_vehicle) {
        return nullptr;
    }

    if (path.isEmpty() || path == QLatin1String("vehicle")) {
        return _vehicle->vehicleFactGroup();
    }
    if (path == QLatin1String("gps")) {
        return _vehicle->gpsFactGroup();
    }
    if (path == QLatin1String("gps2")) {
        return _vehicle->gps2FactGroup();
    }
    if (path == QLatin1String("wind")) {
        return _vehicle->windFactGroup();
    }
    if (path == QLatin1String("vibration")) {
        return _vehicle->vibrationFactGroup();
    }
    if (path == QLatin1String("localPosition")) {
        return _vehicle->localPositionFactGroup();
    }
    if (path == QLatin1String("estimatorStatus")) {
        return _vehicle->estimatorStatusFactGroup();
    }
    if (path == QLatin1String("terrain")) {
        return _vehicle->terrainFactGroup();
    }
    if (path == QLatin1String("temperature")) {
        return _vehicle->temperatureFactGroup();
    }
    if (path == QLatin1String("clock")) {
        return _vehicle->clockFactGroup();
    }
    if (path == QLatin1String("setpoint")) {
        return _vehicle->setpointFactGroup();
    }
    if (path == QLatin1String("distanceSensors")) {
        return _vehicle->distanceSensorFactGroup();
    }
    if (path == QLatin1String("battery")) {
        QmlObjectListModel *batteries = _vehicle->batteries();
        if (batteries && batteries->count() > 0) {
            return qobject_cast<FactGroup *>(batteries->get(0));
        }
        return nullptr;
    }

    qDebug() << "[DDSDataInjector] Unknown fact group:" << path;
    return nullptr;
}

void DDSDataInjector::_injectField(const QString &factGroupPath,
                                    const QString &factName,
                                    const QVariant &value)
{
    FactGroup *group = _resolveFactGroup(factGroupPath);
    if (!group) {
        return;
    }

    Fact *fact = group->getFact(factName);
    if (fact) {
        fact->setRawValue(value);
        _factsUpdated++;
        emit factUpdated(factGroupPath, factName, value);
    }
}

void DDSDataInjector::_updateVehicleCoordinate(const QHash<QString, QVariant> &fields)
{
    // Update Vehicle::_coordinate from vehicle_global_position fields
    const auto latIt = fields.constFind(QStringLiteral("lat"));
    const auto lonIt = fields.constFind(QStringLiteral("lon"));
    const auto altIt = fields.constFind(QStringLiteral("alt"));

    if (latIt == fields.constEnd() || lonIt == fields.constEnd()) {
        return;
    }

    bool latOk = false, lonOk = false;
    const double lat = latIt->toDouble(&latOk);
    const double lon = lonIt->toDouble(&lonOk);

    if (!latOk || !lonOk) {
        return;
    }

    // Sanity check: PX4 global position uses decimal degrees directly
    if (lat == 0.0 && lon == 0.0) {
        return;
    }

    double alt = 0.0;
    if (altIt != fields.constEnd()) {
        alt = altIt->toDouble();
    }

    QGeoCoordinate coord(lat, lon, alt);
    if (coord.isValid()) {
        _vehicle->setCoordinateFromDDS(coord);
    }
}

static uint32_t _navStateToCustomMode(int navState)
{
    // Mirrors PX4's get_px4_custom_mode() — convert nav_state to custom_mode
    // so that PX4FirmwarePlugin::flightMode() returns the correct string.
    switch (navState) {
    case  0: return PX4CustomMode::MANUAL;
    case  1: return PX4CustomMode::ALTCTL;
    case  2: return PX4CustomMode::POSCTL_POSCTL;
    case  3: return PX4CustomMode::AUTO_MISSION;
    case  4: return PX4CustomMode::AUTO_LOITER;
    case  5: return PX4CustomMode::AUTO_RTL;
    case 10: return PX4CustomMode::ACRO;
    case 12: return PX4CustomMode::AUTO_LAND;       // DESCEND
    case 14: return PX4CustomMode::OFFBOARD;
    case 15: return PX4CustomMode::STABILIZED;
    case 17: return PX4CustomMode::AUTO_TAKEOFF;
    case 18: return PX4CustomMode::AUTO_LAND;
    case 19: return PX4CustomMode::AUTO_FOLLOW_TARGET;
    case 20: return PX4CustomMode::AUTO_PRECLAND;
    case 21: return PX4CustomMode::POSCTL_ORBIT;
    default: return 0;
    }
}

void DDSDataInjector::_updateVehicleState(const QHash<QString, QVariant> &fields)
{
    const auto armIt = fields.constFind(QStringLiteral("arming_state"));
    if (armIt != fields.constEnd()) {
        _armingState = armIt->toInt();

        const bool armed = (_armingState == 2);
        if (_vehicle->_armed != armed) {
            _vehicle->_armed = armed;
            emit _vehicle->armedChanged(armed);
        }
    }

    const auto navIt = fields.constFind(QStringLiteral("nav_state"));
    if (navIt != fields.constEnd()) {
        const int newNavState = navIt->toInt();
        if (newNavState != _navState) {
            _navState = newNavState;

            const uint32_t customMode = _navStateToCustomMode(_navState);
            const uint8_t  baseMode   = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;

            QString previousFlightMode;
            if (_vehicle->_base_mode != 0 || _vehicle->_custom_mode != 0) {
                previousFlightMode = _vehicle->flightMode();
            }
            _vehicle->_base_mode   = baseMode;
            _vehicle->_custom_mode = customMode;

            const QString newFlightMode = _vehicle->flightMode();
            if (previousFlightMode != newFlightMode) {
                emit _vehicle->flightModeChanged(newFlightMode);
            }
        }
    }
}

void DDSDataInjector::_updateHomePosition(const QHash<QString, QVariant> &fields)
{
    const auto latIt = fields.constFind(QStringLiteral("lat"));
    const auto lonIt = fields.constFind(QStringLiteral("lon"));
    const auto altIt = fields.constFind(QStringLiteral("alt"));

    if (latIt == fields.constEnd() || lonIt == fields.constEnd()) {
        return;
    }

    bool latOk = false, lonOk = false;
    const double lat = latIt->toDouble(&latOk);
    const double lon = lonIt->toDouble(&lonOk);
    if (!latOk || !lonOk || (lat == 0.0 && lon == 0.0)) {
        return;
    }

    double alt = 0.0;
    if (altIt != fields.constEnd()) {
        alt = altIt->toDouble();
    }

    QGeoCoordinate homeCoord(lat, lon, alt);
    if (homeCoord.isValid()) {
        _vehicle->_setHomePosition(homeCoord);
    }
}

void DDSDataInjector::_updateLandDetected(const QHash<QString, QVariant> &fields)
{
    const auto landedIt = fields.constFind(QStringLiteral("landed"));
    if (landedIt == fields.constEnd()) {
        return;
    }

    const bool landed = landedIt->toBool();
    _vehicle->_setFlying(!landed);
    _vehicle->_setLanding(false);
}

void DDSDataInjector::_updateReadyToFly(const QHash<QString, QVariant> &fields)
{
    // Determine readyToFly from failsafe_flags:
    // If no critical failsafe flags are set and arming_state is STANDBY (1),
    // the vehicle is ready to fly.
    bool hasCriticalFailsafe = false;

    if (fields.value(QStringLiteral("global_position_invalid"), false).toBool() ||
        fields.value(QStringLiteral("local_position_invalid"), false).toBool() ||
        fields.value(QStringLiteral("fd_critical_failure"), false).toBool()) {
        hasCriticalFailsafe = true;
    }

    const bool readyToFly = (_armingState == 1) && !hasCriticalFailsafe;

    if (!_readyToFlySet) {
        _readyToFlySet = true;
        _vehicle->_setReadyToFlyAvailable(true);
    }
    _vehicle->_setReadyToFly(readyToFly);
}

void DDSDataInjector::_handleCommandAck(const QHash<QString, QVariant> &fields)
{
    const uint32_t command = fields.value(QStringLiteral("command"), 0).toUInt();
    const uint8_t result = static_cast<uint8_t>(fields.value(QStringLiteral("result"), 0).toUInt());
    const uint8_t targetSystem = static_cast<uint8_t>(fields.value(QStringLiteral("target_system"), 0).toUInt());

    qInfo() << "[DDSDataInjector] Command ACK: cmd=" << command
            << "result=" << result << "target=" << targetSystem;

    emit commandAckReceived(command, result, targetSystem);
}

void DDSDataInjector::_ensureBatteryExists()
{
    if (_batteryCreated) {
        return;
    }

    QmlObjectListModel *batteries = _vehicle->batteries();
    if (!batteries || batteries->count() > 0) {
        _batteryCreated = true;
        return;
    }

    auto *battery = new BatteryFactGroup(0, batteries);
    batteries->append(battery);
    _batteryCreated = true;
}

#endif // QGC_ENABLE_DDS
