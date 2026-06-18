#ifdef QGC_ENABLE_DDS

#include "DDSTypeRegistry.h"

#include <QtCore/QDebug>

// IDL-generated headers — v1 (default, from src/DDS/idl/)
#include "VehicleAttitude.h"
#include "VehicleGlobalPosition.h"
#include "VehicleLocalPosition.h"
#include "SensorGps.h"
#include "BatteryStatus.h"
#include "VehicleStatus.h"
#include "Wind.h"
#include "VehicleLandDetected.h"
#include "HomePosition.h"
#include "AirspeedValidated.h"
#include "VehicleOdometry.h"
#include "EstimatorStatusFlags.h"
#include "FailsafeFlags.h"
#include "VehicleControlMode.h"
#include "VehicleCommandAck.h"
#include "GimbalDeviceAttitudeStatus.h"
#include "SensorCombined.h"
#include "ManualControlSetpoint.h"
#include "VtolVehicleStatus.h"
#include "TransponderReport.h"

// IDL-generated headers — v4 (from src/DDS/idl/v4/)
#include "VehicleStatus_v4.h"

DDSTypeRegistry::DDSTypeRegistry()
{
    _registerBuiltinTypes();
    _registerV4Types();
}

void DDSTypeRegistry::setIdlVersion(const QString &version)
{
    _activeVersion = version;
    qInfo() << "[DDSTypeRegistry] Active IDL version set to" << version;
}

const DDSTypeEntry *DDSTypeRegistry::typeEntry(const QString &typeName) const
{
    return typeEntry(typeName, _activeVersion);
}

const DDSTypeEntry *DDSTypeRegistry::typeEntry(const QString &typeName,
                                                const QString &version) const
{
    // Try the requested version first
    auto it = _entries.constFind(_key(typeName, version));
    if (it != _entries.constEnd()) {
        return &it.value();
    }
    // Fall back to v1 if the requested version is not available
    if (version != QLatin1String("v1")) {
        it = _entries.constFind(_key(typeName, QStringLiteral("v1")));
        if (it != _entries.constEnd()) {
            return &it.value();
        }
    }
    return nullptr;
}

bool DDSTypeRegistry::hasType(const QString &typeName) const
{
    for (auto it = _entries.constBegin(); it != _entries.constEnd(); ++it) {
        if (it.key().startsWith(typeName)) {
            return true;
        }
    }
    return false;
}

QStringList DDSTypeRegistry::versionsForType(const QString &typeName) const
{
    QStringList versions;
    for (auto it = _entries.constBegin(); it != _entries.constEnd(); ++it) {
        const QString &key = it.key();
        const int sep = key.indexOf(QChar::fromLatin1('\0'));
        if (sep >= 0 && key.left(sep) == typeName) {
            versions.append(key.mid(sep + 1));
        }
    }
    return versions;
}

int DDSTypeRegistry::count() const
{
    return _entries.size();
}

// ---------------------------------------------------------------------------
// Field extractors — v1 (default)
// ---------------------------------------------------------------------------

static QHash<QString, QVariant> extractVehicleAttitude(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleAttitude_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("q[0]"), QVariant(static_cast<double>(s->q[0]))},
        {QStringLiteral("q[1]"), QVariant(static_cast<double>(s->q[1]))},
        {QStringLiteral("q[2]"), QVariant(static_cast<double>(s->q[2]))},
        {QStringLiteral("q[3]"), QVariant(static_cast<double>(s->q[3]))},
    };
}

static QHash<QString, QVariant> extractVehicleGlobalPosition(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleGlobalPosition_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("lat"),       QVariant(s->lat)},
        {QStringLiteral("lon"),       QVariant(s->lon)},
        {QStringLiteral("alt"),       QVariant(static_cast<double>(s->alt))},
        {QStringLiteral("alt_ellipsoid"), QVariant(static_cast<double>(s->alt_ellipsoid))},
        {QStringLiteral("eph"),       QVariant(static_cast<double>(s->eph))},
        {QStringLiteral("epv"),       QVariant(static_cast<double>(s->epv))},
        {QStringLiteral("terrain_alt"), QVariant(static_cast<double>(s->terrain_alt))},
    };
}

static QHash<QString, QVariant> extractVehicleLocalPosition(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleLocalPosition_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("x"),  QVariant(static_cast<double>(s->x))},
        {QStringLiteral("y"),  QVariant(static_cast<double>(s->y))},
        {QStringLiteral("z"),  QVariant(static_cast<double>(s->z))},
        {QStringLiteral("vx"), QVariant(static_cast<double>(s->vx))},
        {QStringLiteral("vy"), QVariant(static_cast<double>(s->vy))},
        {QStringLiteral("vz"), QVariant(static_cast<double>(s->vz))},
        {QStringLiteral("heading"), QVariant(static_cast<double>(s->heading))},
        {QStringLiteral("ref_lat"), QVariant(s->ref_lat)},
        {QStringLiteral("ref_lon"), QVariant(s->ref_lon)},
        {QStringLiteral("ref_alt"), QVariant(static_cast<double>(s->ref_alt))},
    };
}

static QHash<QString, QVariant> extractSensorGps(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__SensorGps_ *>(sample);
    return {
        {QStringLiteral("timestamp"),      QVariant::fromValue(s->timestamp)},
        {QStringLiteral("latitude_deg"),   QVariant(s->latitude_deg)},
        {QStringLiteral("longitude_deg"),  QVariant(s->longitude_deg)},
        {QStringLiteral("altitude_msl_m"), QVariant(s->altitude_msl_m)},
        {QStringLiteral("fix_type"),       QVariant(static_cast<int>(s->fix_type))},
        {QStringLiteral("eph"),            QVariant(static_cast<double>(s->eph))},
        {QStringLiteral("epv"),            QVariant(static_cast<double>(s->epv))},
        {QStringLiteral("hdop"),           QVariant(static_cast<double>(s->hdop))},
        {QStringLiteral("vdop"),           QVariant(static_cast<double>(s->vdop))},
        {QStringLiteral("vel_m_s"),        QVariant(static_cast<double>(s->vel_m_s))},
        {QStringLiteral("satellites_used"), QVariant(static_cast<int>(s->satellites_used))},
        {QStringLiteral("vel_n_m_s"),      QVariant(static_cast<double>(s->vel_n_m_s))},
        {QStringLiteral("vel_e_m_s"),      QVariant(static_cast<double>(s->vel_e_m_s))},
        {QStringLiteral("vel_d_m_s"),      QVariant(static_cast<double>(s->vel_d_m_s))},
    };
}

static QHash<QString, QVariant> extractBatteryStatus(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__BatteryStatus_ *>(sample);
    return {
        {QStringLiteral("timestamp"),    QVariant::fromValue(s->timestamp)},
        {QStringLiteral("connected"),    QVariant(s->connected)},
        {QStringLiteral("voltage_v"),    QVariant(static_cast<double>(s->voltage_v))},
        {QStringLiteral("current_a"),    QVariant(static_cast<double>(s->current_a))},
        {QStringLiteral("remaining"),    QVariant(static_cast<double>(s->remaining))},
        {QStringLiteral("discharged_mah"), QVariant(static_cast<double>(s->discharged_mah))},
        {QStringLiteral("temperature"),  QVariant(static_cast<double>(s->temperature))},
        {QStringLiteral("cell_count"),   QVariant(static_cast<int>(s->cell_count))},
        {QStringLiteral("time_remaining_s"), QVariant(static_cast<double>(s->time_remaining_s))},
        {QStringLiteral("warning"),      QVariant(static_cast<int>(s->warning))},
        {QStringLiteral("id"),           QVariant(static_cast<int>(s->id))},
    };
}

static QHash<QString, QVariant> extractVehicleStatus(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleStatus_ *>(sample);
    return {
        {QStringLiteral("timestamp"),      QVariant::fromValue(s->timestamp)},
        {QStringLiteral("arming_state"),   QVariant(static_cast<int>(s->arming_state))},
        {QStringLiteral("nav_state"),      QVariant(static_cast<int>(s->nav_state))},
        {QStringLiteral("vehicle_type"),   QVariant(static_cast<int>(s->vehicle_type))},
        {QStringLiteral("hil_state"),      QVariant(static_cast<int>(s->hil_state))},
        {QStringLiteral("armed_time"),     QVariant::fromValue(s->armed_time)},
        {QStringLiteral("takeoff_time"),   QVariant::fromValue(s->takeoff_time)},
    };
}

static QHash<QString, QVariant> extractWind(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__Wind_ *>(sample);
    return {
        {QStringLiteral("timestamp"),       QVariant::fromValue(s->timestamp)},
        {QStringLiteral("windspeed_north"), QVariant(static_cast<double>(s->windspeed_north))},
        {QStringLiteral("windspeed_east"),  QVariant(static_cast<double>(s->windspeed_east))},
    };
}

static QHash<QString, QVariant> extractVehicleLandDetected(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleLandDetected_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("landed"),    QVariant(s->landed)},
        {QStringLiteral("freefall"),  QVariant(s->freefall)},
        {QStringLiteral("ground_contact"), QVariant(s->ground_contact)},
    };
}

static QHash<QString, QVariant> extractHomePosition(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__HomePosition_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("lat"),       QVariant(s->lat)},
        {QStringLiteral("lon"),       QVariant(s->lon)},
        {QStringLiteral("alt"),       QVariant(static_cast<double>(s->alt))},
        {QStringLiteral("valid_alt"), QVariant(s->valid_alt)},
        {QStringLiteral("valid_hpos"), QVariant(s->valid_hpos)},
    };
}

static QHash<QString, QVariant> extractAirspeedValidated(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__AirspeedValidated_ *>(sample);
    return {
        {QStringLiteral("timestamp"),              QVariant::fromValue(s->timestamp)},
        {QStringLiteral("indicated_airspeed_m_s"), QVariant(static_cast<double>(s->indicated_airspeed_m_s))},
        {QStringLiteral("calibrated_airspeed_m_s"), QVariant(static_cast<double>(s->calibrated_airspeed_m_s))},
        {QStringLiteral("true_airspeed_m_s"),      QVariant(static_cast<double>(s->true_airspeed_m_s))},
    };
}

static QHash<QString, QVariant> extractVehicleOdometry(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleOdometry_ *>(sample);
    return {
        {QStringLiteral("timestamp"),  QVariant::fromValue(s->timestamp)},
        {QStringLiteral("position[0]"), QVariant(static_cast<double>(s->position[0]))},
        {QStringLiteral("position[1]"), QVariant(static_cast<double>(s->position[1]))},
        {QStringLiteral("position[2]"), QVariant(static_cast<double>(s->position[2]))},
        {QStringLiteral("q[0]"),       QVariant(static_cast<double>(s->q[0]))},
        {QStringLiteral("q[1]"),       QVariant(static_cast<double>(s->q[1]))},
        {QStringLiteral("q[2]"),       QVariant(static_cast<double>(s->q[2]))},
        {QStringLiteral("q[3]"),       QVariant(static_cast<double>(s->q[3]))},
        {QStringLiteral("velocity[0]"), QVariant(static_cast<double>(s->velocity[0]))},
        {QStringLiteral("velocity[1]"), QVariant(static_cast<double>(s->velocity[1]))},
        {QStringLiteral("velocity[2]"), QVariant(static_cast<double>(s->velocity[2]))},
    };
}

static QHash<QString, QVariant> extractEstimatorStatusFlags(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__EstimatorStatusFlags_ *>(sample);
    return {
        {QStringLiteral("timestamp"),     QVariant::fromValue(s->timestamp)},
        {QStringLiteral("cs_tilt_align"), QVariant(s->cs_tilt_align)},
        {QStringLiteral("cs_yaw_align"),  QVariant(s->cs_yaw_align)},
        {QStringLiteral("cs_gnss_pos"),   QVariant(s->cs_gnss_pos)},
        {QStringLiteral("cs_in_air"),     QVariant(s->cs_in_air)},
        {QStringLiteral("cs_wind"),       QVariant(s->cs_wind)},
        {QStringLiteral("cs_mag_fault"),  QVariant(s->cs_mag_fault)},
    };
}

static QHash<QString, QVariant> extractFailsafeFlags(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__FailsafeFlags_ *>(sample);
    return {
        {QStringLiteral("timestamp"),                    QVariant::fromValue(s->timestamp)},
        {QStringLiteral("manual_control_signal_lost"),   QVariant(s->manual_control_signal_lost)},
        {QStringLiteral("gcs_connection_lost"),          QVariant(s->gcs_connection_lost)},
        {QStringLiteral("battery_warning"),              QVariant(static_cast<int>(s->battery_warning))},
        {QStringLiteral("global_position_invalid"),      QVariant(s->global_position_invalid)},
        {QStringLiteral("local_position_invalid"),       QVariant(s->local_position_invalid)},
        {QStringLiteral("geofence_breached"),            QVariant(s->geofence_breached)},
        {QStringLiteral("fd_critical_failure"),          QVariant(s->fd_critical_failure)},
        {QStringLiteral("home_position_invalid"),        QVariant(s->home_position_invalid)},
    };
}

static QHash<QString, QVariant> extractVehicleControlMode(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleControlMode_ *>(sample);
    return {
        {QStringLiteral("timestamp"),                      QVariant::fromValue(s->timestamp)},
        {QStringLiteral("flag_armed"),                     QVariant(s->flag_armed)},
        {QStringLiteral("flag_control_manual_enabled"),    QVariant(s->flag_control_manual_enabled)},
        {QStringLiteral("flag_control_auto_enabled"),      QVariant(s->flag_control_auto_enabled)},
        {QStringLiteral("flag_control_offboard_enabled"),  QVariant(s->flag_control_offboard_enabled)},
        {QStringLiteral("flag_control_position_enabled"),  QVariant(s->flag_control_position_enabled)},
        {QStringLiteral("flag_control_altitude_enabled"),  QVariant(s->flag_control_altitude_enabled)},
        {QStringLiteral("flag_control_attitude_enabled"),  QVariant(s->flag_control_attitude_enabled)},
    };
}

static QHash<QString, QVariant> extractVehicleCommandAck(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VehicleCommandAck_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("command"),   QVariant(static_cast<quint32>(s->command))},
        {QStringLiteral("result"),    QVariant(static_cast<int>(s->result))},
    };
}

static QHash<QString, QVariant> extractGimbalDeviceAttitudeStatus(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__GimbalDeviceAttitudeStatus_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("q[0]"), QVariant(static_cast<double>(s->q[0]))},
        {QStringLiteral("q[1]"), QVariant(static_cast<double>(s->q[1]))},
        {QStringLiteral("q[2]"), QVariant(static_cast<double>(s->q[2]))},
        {QStringLiteral("q[3]"), QVariant(static_cast<double>(s->q[3]))},
        {QStringLiteral("device_flags"), QVariant(static_cast<int>(s->device_flags))},
    };
}

static QHash<QString, QVariant> extractSensorCombined(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__SensorCombined_ *>(sample);
    return {
        {QStringLiteral("timestamp"),         QVariant::fromValue(s->timestamp)},
        {QStringLiteral("gyro_rad[0]"),       QVariant(static_cast<double>(s->gyro_rad[0]))},
        {QStringLiteral("gyro_rad[1]"),       QVariant(static_cast<double>(s->gyro_rad[1]))},
        {QStringLiteral("gyro_rad[2]"),       QVariant(static_cast<double>(s->gyro_rad[2]))},
        {QStringLiteral("accelerometer_m_s2[0]"), QVariant(static_cast<double>(s->accelerometer_m_s2[0]))},
        {QStringLiteral("accelerometer_m_s2[1]"), QVariant(static_cast<double>(s->accelerometer_m_s2[1]))},
        {QStringLiteral("accelerometer_m_s2[2]"), QVariant(static_cast<double>(s->accelerometer_m_s2[2]))},
    };
}

static QHash<QString, QVariant> extractManualControlSetpoint(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__ManualControlSetpoint_ *>(sample);
    return {
        {QStringLiteral("timestamp"), QVariant::fromValue(s->timestamp)},
        {QStringLiteral("valid"),     QVariant(s->valid)},
        {QStringLiteral("roll"),      QVariant(static_cast<double>(s->roll))},
        {QStringLiteral("pitch"),     QVariant(static_cast<double>(s->pitch))},
        {QStringLiteral("yaw"),       QVariant(static_cast<double>(s->yaw))},
        {QStringLiteral("throttle"),  QVariant(static_cast<double>(s->throttle))},
    };
}

static QHash<QString, QVariant> extractVtolVehicleStatus(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__VtolVehicleStatus_ *>(sample);
    return {
        {QStringLiteral("timestamp"),              QVariant::fromValue(s->timestamp)},
        {QStringLiteral("vehicle_vtol_state"),     QVariant(static_cast<int>(s->vehicle_vtol_state))},
        {QStringLiteral("fixed_wing_system_failure"), QVariant(s->fixed_wing_system_failure)},
    };
}

static QHash<QString, QVariant> extractTransponderReport(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds__TransponderReport_ *>(sample);
    return {
        {QStringLiteral("timestamp"),    QVariant::fromValue(s->timestamp)},
        {QStringLiteral("icao_address"), QVariant(static_cast<quint32>(s->icao_address))},
        {QStringLiteral("lat"),          QVariant(s->lat)},
        {QStringLiteral("lon"),          QVariant(s->lon)},
        {QStringLiteral("altitude"),     QVariant(static_cast<double>(s->altitude))},
        {QStringLiteral("heading"),      QVariant(static_cast<double>(s->heading))},
        {QStringLiteral("hor_velocity"), QVariant(static_cast<double>(s->hor_velocity))},
        {QStringLiteral("ver_velocity"), QVariant(static_cast<double>(s->ver_velocity))},
        {QStringLiteral("callsign"),     QVariant(QString::fromLatin1(s->callsign, 8).trimmed())},
        {QStringLiteral("squawk"),       QVariant(static_cast<int>(s->squawk))},
    };
}

// ---------------------------------------------------------------------------
// Field extractors — v4
// ---------------------------------------------------------------------------

static QHash<QString, QVariant> extractVehicleStatusV4(const void *sample)
{
    const auto *s = static_cast<const px4_msgs_msg_dds_v4__VehicleStatus_ *>(sample);
    return {
        {QStringLiteral("timestamp"),      QVariant::fromValue(s->timestamp)},
        {QStringLiteral("arming_state"),   QVariant(static_cast<int>(s->arming_state))},
        {QStringLiteral("nav_state"),      QVariant(static_cast<int>(s->nav_state))},
        {QStringLiteral("vehicle_type"),   QVariant(static_cast<int>(s->vehicle_type))},
        {QStringLiteral("hil_state"),      QVariant(static_cast<int>(s->hil_state))},
        {QStringLiteral("armed_time"),     QVariant::fromValue(s->armed_time)},
        {QStringLiteral("takeoff_time"),   QVariant::fromValue(s->takeoff_time)},
        {QStringLiteral("failsafe"),       QVariant(s->failsafe)},
    };
}

// ---------------------------------------------------------------------------
// Registration — v1 (default)
// ---------------------------------------------------------------------------

void DDSTypeRegistry::_registerBuiltinTypes()
{
    const QString v1 = QStringLiteral("v1");

    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleAttitude_"), v1),
                    {&px4_msgs_msg_dds__VehicleAttitude__desc, extractVehicleAttitude});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleGlobalPosition_"), v1),
                    {&px4_msgs_msg_dds__VehicleGlobalPosition__desc, extractVehicleGlobalPosition});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleLocalPosition_"), v1),
                    {&px4_msgs_msg_dds__VehicleLocalPosition__desc, extractVehicleLocalPosition});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::SensorGps_"), v1),
                    {&px4_msgs_msg_dds__SensorGps__desc, extractSensorGps});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::BatteryStatus_"), v1),
                    {&px4_msgs_msg_dds__BatteryStatus__desc, extractBatteryStatus});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleStatus_"), v1),
                    {&px4_msgs_msg_dds__VehicleStatus__desc, extractVehicleStatus});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::Wind_"), v1),
                    {&px4_msgs_msg_dds__Wind__desc, extractWind});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleLandDetected_"), v1),
                    {&px4_msgs_msg_dds__VehicleLandDetected__desc, extractVehicleLandDetected});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::HomePosition_"), v1),
                    {&px4_msgs_msg_dds__HomePosition__desc, extractHomePosition});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::AirspeedValidated_"), v1),
                    {&px4_msgs_msg_dds__AirspeedValidated__desc, extractAirspeedValidated});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleOdometry_"), v1),
                    {&px4_msgs_msg_dds__VehicleOdometry__desc, extractVehicleOdometry});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::EstimatorStatusFlags_"), v1),
                    {&px4_msgs_msg_dds__EstimatorStatusFlags__desc, extractEstimatorStatusFlags});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::FailsafeFlags_"), v1),
                    {&px4_msgs_msg_dds__FailsafeFlags__desc, extractFailsafeFlags});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleControlMode_"), v1),
                    {&px4_msgs_msg_dds__VehicleControlMode__desc, extractVehicleControlMode});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleCommandAck_"), v1),
                    {&px4_msgs_msg_dds__VehicleCommandAck__desc, extractVehicleCommandAck});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::GimbalDeviceAttitudeStatus_"), v1),
                    {&px4_msgs_msg_dds__GimbalDeviceAttitudeStatus__desc, extractGimbalDeviceAttitudeStatus});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::SensorCombined_"), v1),
                    {&px4_msgs_msg_dds__SensorCombined__desc, extractSensorCombined});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::ManualControlSetpoint_"), v1),
                    {&px4_msgs_msg_dds__ManualControlSetpoint__desc, extractManualControlSetpoint});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VtolVehicleStatus_"), v1),
                    {&px4_msgs_msg_dds__VtolVehicleStatus__desc, extractVtolVehicleStatus});
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::TransponderReport_"), v1),
                    {&px4_msgs_msg_dds__TransponderReport__desc, extractTransponderReport});

    qInfo() << "[DDSTypeRegistry] Registered v1 types:" << 20;
}

// ---------------------------------------------------------------------------
// Registration — v4
// ---------------------------------------------------------------------------

void DDSTypeRegistry::_registerV4Types()
{
    const QString v4 = QStringLiteral("v4");

    // VehicleStatus v4 — different struct layout (older PX4 firmware)
    _entries.insert(_key(QStringLiteral("px4_msgs::msg::dds_::VehicleStatus_"), v4),
                    {&px4_msgs_msg_dds_v4__VehicleStatus__desc, extractVehicleStatusV4});

    qInfo() << "[DDSTypeRegistry] Registered v4 types: 1 (VehicleStatus)";
    qInfo() << "[DDSTypeRegistry] Total registered:" << _entries.size() << "type entries";
}

#endif // QGC_ENABLE_DDS
