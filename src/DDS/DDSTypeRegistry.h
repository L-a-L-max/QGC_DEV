#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <dds/dds.h>

#include <functional>

/// Signature for a function that extracts named fields from a raw DDS sample
/// into a QHash suitable for DDSDataInjector processing.
using DDSFieldExtractorFunc = std::function<QHash<QString, QVariant>(const void *sample)>;

/// Entry in the type registry: pairs a CycloneDDS topic descriptor with
/// a field extractor so that DDSLink can create typed readers and convert
/// received samples into key/value maps.
struct DDSTypeEntry {
    const dds_topic_descriptor_t *descriptor = nullptr;
    DDSFieldExtractorFunc         extractor;
};

/// DDSTypeRegistry maps DDS type name strings (as they appear in the JSON
/// mapping table, e.g. "px4_msgs::msg::VehicleAttitude") to their CycloneDDS
/// topic descriptors and field extraction functions.
///
/// It is populated once at startup with all compiled-in IDL types.
class DDSTypeRegistry
{
public:
    DDSTypeRegistry();

    /// Look up a type entry by its full type name.
    /// Returns nullptr if the type is not registered.
    const DDSTypeEntry *typeEntry(const QString &typeName) const;

    /// Check whether a type is registered.
    bool hasType(const QString &typeName) const;

    /// Number of registered types.
    int count() const { return _entries.size(); }

private:
    void _registerBuiltinTypes();

    QHash<QString, DDSTypeEntry> _entries;
};

#endif // QGC_ENABLE_DDS
