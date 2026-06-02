#ifdef QGC_ENABLE_DDS

#include "DDSDataInjector.h"
#include "DDSMappingEngine.h"
#include "DDSTransformRegistry.h"

#include <QtCore/QDebug>

// NOTE: When integrating into QGC, replace these stubs with real includes:
// #include "Vehicle.h"
// #include "Fact.h"
// #include "FactGroup.h"

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

    // Look up topic mapping
    const DDSTopicMapping *mapping = _mappingEngine->topicMapping(topicName);
    if (!mapping) {
        _unmappedSkipped++;
        qDebug() << "[DDSDataInjector] Unmapped topic:" << topicName;
        return;
    }

    // Process each field mapping
    for (const DDSFieldMapping &fieldMapping : mapping->fields) {
        QVariant value;

        if (!fieldMapping.transform.isEmpty()) {
            // Use registered transform function
            DDSTransformFunc func = _transformRegistry->transform(fieldMapping.transform);
            if (func) {
                // For transforms that need the full field set (e.g. quaternion),
                // pass all fields. For simple transforms, inject _value key.
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
            // Direct field mapping (no transform)
            if (!fields.contains(fieldMapping.ddsField)) {
                continue;
            }
            value = fields.value(fieldMapping.ddsField);
        }

        // Apply scale and offset: result = value * scale + offset
        if (fieldMapping.scale != 1.0 || fieldMapping.offset != 0.0) {
            bool ok = false;
            double numVal = value.toDouble(&ok);
            if (ok) {
                numVal = numVal * fieldMapping.scale + fieldMapping.offset;
                value = QVariant(numVal);
            }
        }

        // Inject into Fact system
        const QString &factGroup = fieldMapping.factGroup;
        const QString &factName = fieldMapping.factName;
        _injectField(factGroup, factName, value);
    }
}

void DDSDataInjector::_injectField(const QString &factGroupPath,
                                    const QString &factName,
                                    const QVariant &value)
{
    // TODO(P1-integration): When integrating into QGC, implement as:
    //
    //   if (!_vehicle) return;
    //
    //   // Resolve FactGroup from path
    //   // "vehicle" → _vehicle (which is VehicleFactGroup itself)
    //   // "gps"     → _vehicle->gpsFactGroup()
    //   // "battery" → _vehicle->battery1FactGroup()
    //   // etc.
    //
    //   FactGroup *group = nullptr;
    //   if (factGroupPath == "vehicle") {
    //       group = _vehicle;
    //   } else {
    //       group = _vehicle->getFactGroup(factGroupPath);
    //   }
    //
    //   if (!group) {
    //       qDebug() << "[DDSDataInjector] FactGroup not found:" << factGroupPath;
    //       return;
    //   }
    //
    //   Fact *fact = group->getFact(factName);
    //   if (!fact) {
    //       qDebug() << "[DDSDataInjector] Fact not found:" << factGroupPath << "/" << factName;
    //       return;
    //   }
    //
    //   fact->setRawValue(value);

    _factsUpdated++;
    emit factUpdated(factGroupPath, factName, value);

    qDebug() << "[DDSDataInjector]" << factGroupPath << "/" << factName << "=" << value;
}

#endif // QGC_ENABLE_DDS
