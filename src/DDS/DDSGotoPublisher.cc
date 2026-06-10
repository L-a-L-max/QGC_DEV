#ifdef QGC_ENABLE_DDS

#include "DDSGotoPublisher.h"
#include "GotoSetpoint.h"

#include <QtCore/QDebug>
#include <QtCore/QtMath>

#include <cmath>
#include <cstring>

static constexpr double kEarthRadius = 6371000.0;

DDSGotoPublisher::DDSGotoPublisher(QObject *parent)
    : QObject(parent)
{
}

DDSGotoPublisher::~DDSGotoPublisher()
{
    deinit();
}

bool DDSGotoPublisher::init(dds_entity_t participant, const QString &namespacePrefix)
{
    if (_writer > 0) {
        return true;
    }

    const QString topicName = namespacePrefix + QStringLiteral("fmu/in/goto_setpoint");

    _topic = dds_create_topic(
        participant,
        &px4_msgs_msg_dds__GotoSetpoint__desc,
        topicName.toUtf8().constData(),
        nullptr, nullptr);

    if (_topic < 0) {
        qWarning() << "[DDSGoto] Failed to create topic:" << topicName;
        _topic = DDS_ENTITY_NIL;
        return false;
    }

    dds_qos_t *qos = dds_create_qos();
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

    _writer = dds_create_writer(participant, _topic, qos, nullptr);
    dds_delete_qos(qos);

    if (_writer < 0) {
        qWarning() << "[DDSGoto] Failed to create writer:" << topicName;
        _writer = DDS_ENTITY_NIL;
        return false;
    }

    qInfo() << "[DDSGoto] Writer created on" << topicName;
    return true;
}

void DDSGotoPublisher::deinit()
{
    if (_writer > 0) {
        dds_delete(_writer);
        _writer = DDS_ENTITY_NIL;
    }
    if (_topic > 0) {
        dds_delete(_topic);
        _topic = DDS_ENTITY_NIL;
    }
}

void DDSGotoPublisher::geoToNED(double lat, double lon, double alt,
                                double homeLat, double homeLon, double homeAlt,
                                float &north, float &east, float &down)
{
    const double dLat = qDegreesToRadians(lat - homeLat);
    const double dLon = qDegreesToRadians(lon - homeLon);
    const double cosHomeLat = cos(qDegreesToRadians(homeLat));

    north = static_cast<float>(dLat * kEarthRadius);
    east  = static_cast<float>(dLon * kEarthRadius * cosHomeLat);
    down  = static_cast<float>(homeAlt - alt);
}

bool DDSGotoPublisher::sendGoto(double lat, double lon, double altAMSL,
                                double homeLat, double homeLon, double homeAlt,
                                float maxHSpeed, float maxVSpeed,
                                float heading, float maxHeadingRate)
{
    if (_writer <= 0) {
        qWarning() << "[DDSGoto] sendGoto called but writer not ready";
        return false;
    }

    float n, e, d;
    geoToNED(lat, lon, altAMSL, homeLat, homeLon, homeAlt, n, e, d);

    px4_msgs_msg_dds__GotoSetpoint_ msg;
    memset(&msg, 0, sizeof(msg));

    msg.timestamp = 0;
    msg.position[0] = n;
    msg.position[1] = e;
    msg.position[2] = d;

    msg.flag_control_heading = !std::isnan(heading);
    msg.heading = msg.flag_control_heading ? heading : 0.0f;

    msg.flag_set_max_horizontal_speed = (maxHSpeed > 0.0f);
    msg.max_horizontal_speed = msg.flag_set_max_horizontal_speed ? maxHSpeed : 0.0f;

    msg.flag_set_max_vertical_speed = (maxVSpeed > 0.0f);
    msg.max_vertical_speed = msg.flag_set_max_vertical_speed ? maxVSpeed : 0.0f;

    msg.flag_set_max_heading_rate = (maxHeadingRate > 0.0f);
    msg.max_heading_rate = msg.flag_set_max_heading_rate ? maxHeadingRate : 0.0f;

    const dds_return_t rc = dds_write(_writer, &msg);

    qInfo() << "[DDSGoto] sendGoto NED=(" << n << e << d << ")"
            << "hSpd=" << maxHSpeed << "vSpd=" << maxVSpeed
            << "hdg=" << heading << "rc=" << rc;

    return rc == DDS_RETCODE_OK;
}

#endif // QGC_ENABLE_DDS
