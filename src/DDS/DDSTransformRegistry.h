#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <functional>

/// @file DDSTransformRegistry.h
/// @brief Registry of named transform functions for DDS field → Fact conversion.
///
/// Transforms convert raw DDS field values into QGC-compatible Fact values.
/// Each transform is a callable that takes the full field map of a DDS message
/// and returns a single QVariant value for the target Fact.
///
/// Built-in transforms include:
///   - quaternion_to_euler_roll/pitch/yaw  (q[0..3] → degrees)
///   - rad_to_deg                          (radians → degrees)
///   - m_to_ft                             (meters → feet)
///   - ms_to_knots                         (m/s → knots)
///   - pa_to_hpa                           (Pascal → hectoPascal)
///   - ned_lat / ned_lon / ned_alt         (1e-7 integer → decimal degrees)
///   - identity                            (pass-through)

/// Transform function signature.
/// @param fields  All fields from the DDS message (field name → QVariant value)
/// @return Transformed value for the target Fact
using DDSTransformFunc = std::function<QVariant(const QHash<QString, QVariant> &fields)>;

class DDSTransformRegistry : public QObject
{
    Q_OBJECT

public:
    explicit DDSTransformRegistry(QObject *parent = nullptr);
    ~DDSTransformRegistry() override;

    /// Register a named transform function. Overwrites any existing transform
    /// with the same name.
    void registerTransform(const QString &name, DDSTransformFunc func);

    /// Look up a transform by name.
    /// @return The transform function, or nullptr-equivalent if not found.
    DDSTransformFunc transform(const QString &name) const;

    /// Check whether a transform with the given name is registered.
    bool hasTransform(const QString &name) const;

    /// Number of registered transforms.
    int count() const { return _transforms.size(); }

private:
    void _registerBuiltins();

    QHash<QString, DDSTransformFunc> _transforms;
};

#endif // QGC_ENABLE_DDS
