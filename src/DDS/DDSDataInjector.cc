#ifdef QGC_ENABLE_DDS

#include "DDSDataInjector.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"
#include "Vehicle.h"
#include "Fact.h"
#include "FactGroup.h"

#include <QtCore/QDebug>

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

    const DDSTopicMapping *mapping = _mappingEngine->topicMapping(topicName);
    if (!mapping) {
        _unmappedSkipped++;
        qDebug() << "[DDSDataInjector] Unmapped topic:" << topicName;
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

    qDebug() << "[DDSDataInjector] Unknown fact group:" << path;
    return nullptr;
}

void DDSDataInjector::_injectField(const QString &factGroupPath,
                                    const QString &factName,
                                    const QVariant &value)
{
    FactGroup *group = _resolveFactGroup(factGroupPath);
    if (group) {
        Fact *fact = group->getFact(factName);
        if (fact) {
            fact->setRawValue(value);
            _factsUpdated++;
            emit factUpdated(factGroupPath, factName, value);
            return;
        }
    }

    _factsUpdated++;
    emit factUpdated(factGroupPath, factName, value);
}

#endif // QGC_ENABLE_DDS
