#ifdef QGC_ENABLE_DDS

#include "DDSDataInjector.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "Vehicle.h"
#include "Fact.h"
#include "FactGroup.h"
#include "QmlObjectListModel.h"
#include "BatteryFactGroupListModel.h"

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
    if (topicName.contains(QLatin1String("battery_status"))) {
        _ensureBatteryExists();
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

void DDSDataInjector::_updateVehicleState(const QHash<QString, QVariant> &fields)
{
    const auto armIt = fields.constFind(QStringLiteral("arming_state"));
    if (armIt != fields.constEnd()) {
        _armingState = armIt->toInt();
    }

    const auto navIt = fields.constFind(QStringLiteral("nav_state"));
    if (navIt != fields.constEnd()) {
        _navState = navIt->toInt();
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
