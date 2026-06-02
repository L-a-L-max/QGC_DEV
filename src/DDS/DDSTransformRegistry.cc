#ifdef QGC_ENABLE_DDS

#include "DDSTransformRegistry.h"

#include <QtCore/QDebug>
#include <cmath>

// Constants
static constexpr double kRadToDeg = 180.0 / M_PI;
static constexpr double kMToFt    = 3.28084;
static constexpr double kMsToKnots = 1.94384;
static constexpr double kPaToHpa  = 0.01;

// Helpers to safely extract numeric values from the field map
static double fieldDouble(const QHash<QString, QVariant> &fields, const QString &key, double fallback = 0.0)
{
    auto it = fields.constFind(key);
    if (it != fields.constEnd()) {
        bool ok = false;
        double v = it->toDouble(&ok);
        return ok ? v : fallback;
    }
    return fallback;
}

DDSTransformRegistry::DDSTransformRegistry(QObject *parent)
    : QObject(parent)
{
    _registerBuiltins();
}

DDSTransformRegistry::~DDSTransformRegistry() = default;

void DDSTransformRegistry::registerTransform(const QString &name, DDSTransformFunc func)
{
    _transforms.insert(name, std::move(func));
}

DDSTransformFunc DDSTransformRegistry::transform(const QString &name) const
{
    return _transforms.value(name);
}

bool DDSTransformRegistry::hasTransform(const QString &name) const
{
    return _transforms.contains(name);
}

void DDSTransformRegistry::_registerBuiltins()
{
    // --- Quaternion to Euler angle transforms ---
    // PX4 attitude quaternion: q[0]=w, q[1]=x, q[2]=y, q[3]=z (Hamilton convention)
    // Output: degrees

    registerTransform(QStringLiteral("quaternion_to_euler_roll"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double w = fieldDouble(fields, QStringLiteral("q[0]"), 1.0);
        const double x = fieldDouble(fields, QStringLiteral("q[1]"));
        const double y = fieldDouble(fields, QStringLiteral("q[2]"));
        const double z = fieldDouble(fields, QStringLiteral("q[3]"));

        // roll (x-axis rotation)
        const double sinr_cosp = 2.0 * (w * x + y * z);
        const double cosr_cosp = 1.0 - 2.0 * (x * x + y * y);
        const double roll = std::atan2(sinr_cosp, cosr_cosp) * kRadToDeg;
        return QVariant(roll);
    });

    registerTransform(QStringLiteral("quaternion_to_euler_pitch"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double w = fieldDouble(fields, QStringLiteral("q[0]"), 1.0);
        const double x = fieldDouble(fields, QStringLiteral("q[1]"));
        const double y = fieldDouble(fields, QStringLiteral("q[2]"));
        const double z = fieldDouble(fields, QStringLiteral("q[3]"));

        // pitch (y-axis rotation)
        const double sinp = 2.0 * (w * y - z * x);
        double pitch;
        if (std::abs(sinp) >= 1.0) {
            pitch = std::copysign(M_PI / 2.0, sinp) * kRadToDeg;  // gimbal lock
        } else {
            pitch = std::asin(sinp) * kRadToDeg;
        }
        return QVariant(pitch);
    });

    registerTransform(QStringLiteral("quaternion_to_euler_yaw"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double w = fieldDouble(fields, QStringLiteral("q[0]"), 1.0);
        const double x = fieldDouble(fields, QStringLiteral("q[1]"));
        const double y = fieldDouble(fields, QStringLiteral("q[2]"));
        const double z = fieldDouble(fields, QStringLiteral("q[3]"));

        // yaw (z-axis rotation)
        const double siny_cosp = 2.0 * (w * z + x * y);
        const double cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
        double yaw = std::atan2(siny_cosp, cosy_cosp) * kRadToDeg;

        // Normalize to [0, 360) for heading display
        if (yaw < 0.0) {
            yaw += 360.0;
        }
        return QVariant(yaw);
    });

    // --- Simple unit transforms ---

    registerTransform(QStringLiteral("rad_to_deg"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double val = fieldDouble(fields, QStringLiteral("_value"));
        return QVariant(val * kRadToDeg);
    });

    registerTransform(QStringLiteral("m_to_ft"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double val = fieldDouble(fields, QStringLiteral("_value"));
        return QVariant(val * kMToFt);
    });

    registerTransform(QStringLiteral("ms_to_knots"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double val = fieldDouble(fields, QStringLiteral("_value"));
        return QVariant(val * kMsToKnots);
    });

    registerTransform(QStringLiteral("pa_to_hpa"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double val = fieldDouble(fields, QStringLiteral("_value"));
        return QVariant(val * kPaToHpa);
    });

    // --- GPS coordinate transforms ---
    // PX4 publishes lat/lon as 1e-7 integers (int32). Convert to decimal degrees.

    registerTransform(QStringLiteral("gps_latlon_1e7"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double val = fieldDouble(fields, QStringLiteral("_value"));
        return QVariant(val * 1e-7);
    });

    // --- NED to ENU coordinate transform ---
    // PX4 uses NED (North-East-Down), QGC expects altitude positive-up.
    // This negates the value (e.g., z_ned = -altitude).

    registerTransform(QStringLiteral("negate"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double val = fieldDouble(fields, QStringLiteral("_value"));
        return QVariant(-val);
    });

    // --- Ground speed from vx, vy ---
    registerTransform(QStringLiteral("ground_speed_from_vxy"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double vx = fieldDouble(fields, QStringLiteral("vx"));
        const double vy = fieldDouble(fields, QStringLiteral("vy"));
        return QVariant(std::sqrt(vx * vx + vy * vy));
    });

    // --- Course over ground from vx, vy ---
    registerTransform(QStringLiteral("course_over_ground_from_vxy"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        const double vx = fieldDouble(fields, QStringLiteral("vx"));
        const double vy = fieldDouble(fields, QStringLiteral("vy"));
        double cog = std::atan2(vy, vx) * kRadToDeg;
        if (cog < 0.0) {
            cog += 360.0;
        }
        return QVariant(cog);
    });

    // --- Identity (pass-through) ---
    registerTransform(QStringLiteral("identity"), [](const QHash<QString, QVariant> &fields) -> QVariant {
        return fields.value(QStringLiteral("_value"));
    });

    qInfo() << "[TransformRegistry] Registered" << _transforms.size() << "built-in transforms";
}

#endif // QGC_ENABLE_DDS
