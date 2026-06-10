#pragma once

#include <QtPositioning/QGeoCoordinate>

#include <cmath>

/// A single DDS mission waypoint with optional speed/heading/hover parameters.
struct DDSWaypoint {
    double latitude  = 0.0;
    double longitude = 0.0;
    float  altitude  = 10.0f;    ///< Relative to home (meters)
    float  speed     = -1.0f;    ///< Max horizontal speed (m/s), <=0 = default
    float  heading   = NAN;      ///< Target heading (degrees), NaN = don't control
    float  hoverTime = 0.0f;     ///< Seconds to hover after arriving, 0 = none

    QGeoCoordinate coordinate() const {
        return QGeoCoordinate(latitude, longitude, static_cast<double>(altitude));
    }
};
