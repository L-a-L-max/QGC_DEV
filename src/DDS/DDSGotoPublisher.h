#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>

#include <dds/dds.h>

/// Publishes GotoSetpoint messages to PX4 via DDS topic fmu/in/goto_setpoint.
///
/// Converts WGS84 lat/lon/alt to NED local coordinates using the vehicle's
/// home position as the origin, then publishes a single goto command with
/// optional speed and heading constraints.
class DDSGotoPublisher : public QObject
{
    Q_OBJECT

public:
    explicit DDSGotoPublisher(QObject *parent = nullptr);
    ~DDSGotoPublisher() override;

    bool init(dds_entity_t participant, const QString &namespacePrefix);
    void deinit();

    bool isReady() const { return _writer > 0; }

    /// Send a goto command. All coordinates are WGS84; NED conversion is done internally.
    /// @param lat/lon        Target latitude/longitude (degrees)
    /// @param altAMSL        Target altitude AMSL (meters)
    /// @param homeLat/Lon    Home position (degrees)
    /// @param homeAlt        Home altitude AMSL (meters)
    /// @param maxHSpeed      Max horizontal speed (m/s), <0 to use default
    /// @param maxVSpeed      Max vertical speed (m/s), <0 to use default
    /// @param heading        Target heading (radians, [-pi,pi]), NaN to not control heading
    /// @param maxHeadingRate Max heading rate (rad/s), <0 to use default
    bool sendGoto(double lat, double lon, double altAMSL,
                  double homeLat, double homeLon, double homeAlt,
                  float maxHSpeed = -1.0f,
                  float maxVSpeed = -1.0f,
                  float heading = NAN,
                  float maxHeadingRate = -1.0f);

    /// Convert WGS84 coordinates to NED relative to a home position.
    static void geoToNED(double lat, double lon, double alt,
                         double homeLat, double homeLon, double homeAlt,
                         float &north, float &east, float &down);

private:
    dds_entity_t _writer = DDS_ENTITY_NIL;
    dds_entity_t _topic  = DDS_ENTITY_NIL;
};

#endif // QGC_ENABLE_DDS
