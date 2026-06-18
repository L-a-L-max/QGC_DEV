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
/// Supports multiple IDL versions: types can be registered with a version tag
/// (e.g. "v1", "v4") and looked up by (typeName, version) pair.  The default
/// version is "v1".
class DDSTypeRegistry
{
public:
    DDSTypeRegistry();

    /// Set the active IDL version for lookups via the single-argument typeEntry().
    void setIdlVersion(const QString &version);
    QString idlVersion() const { return _activeVersion; }

    /// Look up a type entry by its full type name, using the active IDL version.
    /// Falls back to "v1" if the requested version is not registered.
    const DDSTypeEntry *typeEntry(const QString &typeName) const;

    /// Look up a type entry by its full type name and explicit version.
    const DDSTypeEntry *typeEntry(const QString &typeName, const QString &version) const;

    /// Check whether a type is registered (any version).
    bool hasType(const QString &typeName) const;

    /// Available IDL versions for a given type name.
    QStringList versionsForType(const QString &typeName) const;

    /// Number of registered types (counting each version separately).
    int count() const;

private:
    void _registerBuiltinTypes();
    void _registerV4Types();

    // Composite key: "typeName\0version" → DDSTypeEntry
    QHash<QString, DDSTypeEntry> _entries;
    QString _activeVersion = QStringLiteral("v1");

    static QString _key(const QString &typeName, const QString &version)
    {
        return typeName + QChar::fromLatin1('\0') + version;
    }
};

#endif // QGC_ENABLE_DDS
