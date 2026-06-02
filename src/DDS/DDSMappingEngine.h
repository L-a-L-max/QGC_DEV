#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

/// @file DDSMappingEngine.h
/// @brief Runtime engine that resolves DDS topic/field names to QGC Fact paths.
///
/// The mapping engine loads a JSON mapping table (one per vendor) and provides
/// O(1) lookup from (DDS topic name, DDS field name) → (FactGroup path, Fact name).
///
/// Mapping table structure (see resources/dds_mappings/_default.json):
/// {
///   "vendor": "px4_standard",
///   "version": "1.0",
///   "topics": [
///     {
///       "dds_topic": "/fmu/out/vehicle_attitude",
///       "fact_group": "vehicle",
///       "fields": [
///         {
///           "dds_field": "q[0]",
///           "fact_name": "roll",
///           "transform": "quaternion_to_euler_roll"
///         }, ...
///       ]
///     }, ...
///   ]
/// }

/// A single field mapping entry.
struct DDSFieldMapping {
    QString ddsField;       // field name in DDS message (e.g. "q[0]", "lat")
    QString factGroup;      // target FactGroup path (e.g. "vehicle", "gps")
    QString factName;       // target Fact name (e.g. "roll", "lat")
    QString transform;      // transform function name (empty = direct assign)
    double  scale  = 1.0;   // linear scale factor (applied before transform)
    double  offset = 0.0;   // linear offset (applied after scale)
};

/// A topic-level mapping: one DDS topic → multiple field mappings.
struct DDSTopicMapping {
    QString ddsTopicName;               // e.g. "/fmu/out/vehicle_attitude"
    QString defaultFactGroup;           // default FactGroup for fields that don't override
    QVector<DDSFieldMapping> fields;    // field-level mappings
};

class DDSMappingEngine : public QObject
{
    Q_OBJECT

public:
    explicit DDSMappingEngine(QObject *parent = nullptr);
    ~DDSMappingEngine() override;

    /// Load a mapping table from the dds_mappings resource directory.
    /// @param mappingName  File name without .json suffix (e.g. "_default", "vendor_dji")
    /// @return true if loaded successfully
    bool loadMapping(const QString &mappingName);

    /// Load a mapping table from a JSON object (for testing or runtime injection).
    bool loadMappingFromJson(const QJsonObject &root);

    /// Load and merge a vendor-specific overlay on top of the current mapping.
    /// Vendor mappings override matching topics/fields from the base mapping.
    bool loadVendorOverlay(const QString &vendorName);

    /// Look up the topic mapping for a given DDS topic name.
    /// @return nullptr if topic is not in the mapping table.
    const DDSTopicMapping *topicMapping(const QString &ddsTopicName) const;

    /// Get all mapped DDS topic names.
    QStringList allTopicNames() const;

    /// Total number of mapped topics.
    int topicCount() const { return _topicMap.size(); }

    /// Total number of mapped fields across all topics.
    int fieldCount() const;

    /// Vendor name from the loaded mapping.
    QString vendorName() const { return _vendorName; }

    /// Mapping version string.
    QString version() const { return _version; }

private:
    bool _parseTopic(const QJsonObject &topicObj);
    DDSFieldMapping _parseField(const QJsonObject &fieldObj, const QString &defaultGroup) const;
    QString _resolveFilePath(const QString &mappingName) const;

    // Primary lookup: DDS topic name → TopicMapping
    QHash<QString, DDSTopicMapping> _topicMap;

    QString _vendorName;
    QString _version;
};

#endif // QGC_ENABLE_DDS
