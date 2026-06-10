#ifdef QGC_ENABLE_DDS

#include "DDSMissionManager.h"
#include "DDSGotoPublisher.h"
#include "DDSCommandPublisher.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtPositioning/QGeoCoordinate>

#include <cmath>

DDSMissionManager::DDSMissionManager(QObject *parent)
    : QObject(parent)
{
    _arrivalCheckTimer.setInterval(200);  // 5 Hz position check
    connect(&_arrivalCheckTimer, &QTimer::timeout, this, &DDSMissionManager::_checkArrival);

    _hoverTimer.setSingleShot(true);
    connect(&_hoverTimer, &QTimer::timeout, this, &DDSMissionManager::_advanceToNext);

    // Re-send goto periodically to keep PX4 in navigation mode
    _resendTimer.setInterval(2000);
    connect(&_resendTimer, &QTimer::timeout, this, &DDSMissionManager::_sendCurrentWaypoint);
}

DDSMissionManager::~DDSMissionManager() = default;

void DDSMissionManager::setGotoPublisher(DDSGotoPublisher *pub)
{
    _gotoPub = pub;
}

void DDSMissionManager::setCommandPublisher(DDSCommandPublisher *pub)
{
    _cmdPub = pub;
}

void DDSMissionManager::setEndAction(EndAction action)
{
    if (_endAction != action) {
        _endAction = action;
        emit endActionChanged();
    }
}

// ---- Waypoint Management ----

void DDSMissionManager::addWaypoint(double lat, double lon, float alt, float speed, float heading, float hover)
{
    DDSWaypoint wp;
    wp.latitude  = lat;
    wp.longitude = lon;
    wp.altitude  = alt;
    wp.speed     = speed;
    wp.heading   = heading;
    wp.hoverTime = hover;
    _waypoints.append(wp);
    emit waypointsChanged();
}

void DDSMissionManager::insertWaypoint(int index, double lat, double lon, float alt, float speed, float heading, float hover)
{
    if (index < 0 || index > _waypoints.size()) return;
    DDSWaypoint wp;
    wp.latitude  = lat;
    wp.longitude = lon;
    wp.altitude  = alt;
    wp.speed     = speed;
    wp.heading   = heading;
    wp.hoverTime = hover;
    _waypoints.insert(index, wp);
    emit waypointsChanged();
}

void DDSMissionManager::removeWaypoint(int index)
{
    if (index < 0 || index >= _waypoints.size()) return;
    _waypoints.removeAt(index);
    emit waypointsChanged();
}

void DDSMissionManager::clearWaypoints()
{
    _waypoints.clear();
    emit waypointsChanged();
}

void DDSMissionManager::updateWaypoint(int index, double lat, double lon, float alt, float speed, float heading, float hover)
{
    if (index < 0 || index >= _waypoints.size()) return;
    DDSWaypoint &wp = _waypoints[index];
    wp.latitude  = lat;
    wp.longitude = lon;
    wp.altitude  = alt;
    wp.speed     = speed;
    wp.heading   = heading;
    wp.hoverTime = hover;
    emit waypointsChanged();
}

// ---- Mission Control ----

void DDSMissionManager::startMission()
{
    if (_waypoints.isEmpty()) {
        emit missionError(QStringLiteral("No waypoints defined"));
        return;
    }
    if (!_gotoPub || !_gotoPub->isReady()) {
        emit missionError(QStringLiteral("Goto publisher not ready"));
        return;
    }
    if (!_homeValid) {
        emit missionError(QStringLiteral("Home position not set"));
        return;
    }

    _currentIndex = 0;
    emit currentWaypointChanged();
    _setState(Running);
    _sendCurrentWaypoint();
    _arrivalCheckTimer.start();
    _resendTimer.start();
    qInfo() << "[DDSMission] Mission started with" << _waypoints.size() << "waypoints";
}

void DDSMissionManager::pauseMission()
{
    if (_state != Running && _state != Hovering) return;

    _arrivalCheckTimer.stop();
    _resendTimer.stop();
    _hoverTimer.stop();
    _setState(Paused);
    qInfo() << "[DDSMission] Mission paused at WP" << _currentIndex;
}

void DDSMissionManager::resumeMission()
{
    if (_state != Paused) return;

    _setState(Running);
    _sendCurrentWaypoint();
    _arrivalCheckTimer.start();
    _resendTimer.start();
    qInfo() << "[DDSMission] Mission resumed at WP" << _currentIndex;
}

void DDSMissionManager::stopMission()
{
    _arrivalCheckTimer.stop();
    _resendTimer.stop();
    _hoverTimer.stop();
    _currentIndex = -1;
    emit currentWaypointChanged();
    _setState(Idle);
    qInfo() << "[DDSMission] Mission stopped";
}

// ---- Position Updates ----

void DDSMissionManager::updateVehiclePosition(double lat, double lon, double altAMSL)
{
    _vehicleLat = lat;
    _vehicleLon = lon;
    _vehicleAlt = altAMSL;
    emit positionUpdated();
}

void DDSMissionManager::updateHomePosition(double lat, double lon, double altAMSL)
{
    _homeLat = lat;
    _homeLon = lon;
    _homeAlt = altAMSL;
    _homeValid = true;
}

// ---- Save / Load ----

bool DDSMissionManager::saveMission(const QString &filePath)
{
    QJsonArray wpArray;
    for (const auto &wp : _waypoints) {
        QJsonObject obj;
        obj[QStringLiteral("lat")]   = wp.latitude;
        obj[QStringLiteral("lon")]   = wp.longitude;
        obj[QStringLiteral("alt")]   = static_cast<double>(wp.altitude);
        obj[QStringLiteral("speed")] = static_cast<double>(wp.speed);
        if (!std::isnan(wp.heading)) {
            obj[QStringLiteral("heading")] = static_cast<double>(wp.heading);
        }
        obj[QStringLiteral("hover")] = static_cast<double>(wp.hoverTime);
        wpArray.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("version")]   = 1;
    root[QStringLiteral("type")]      = QStringLiteral("DDSMission");
    root[QStringLiteral("endAction")] = static_cast<int>(_endAction);
    root[QStringLiteral("waypoints")] = wpArray;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit missionError(QStringLiteral("Cannot open file: ") + filePath);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    qInfo() << "[DDSMission] Saved" << _waypoints.size() << "waypoints to" << filePath;
    return true;
}

bool DDSMissionManager::loadMission(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit missionError(QStringLiteral("Cannot open file: ") + filePath);
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonObject root = doc.object();
    if (root[QStringLiteral("type")].toString() != QStringLiteral("DDSMission")) {
        emit missionError(QStringLiteral("Invalid mission file type"));
        return false;
    }

    _waypoints.clear();
    _endAction = static_cast<EndAction>(root[QStringLiteral("endAction")].toInt(0));
    emit endActionChanged();

    const QJsonArray wpArray = root[QStringLiteral("waypoints")].toArray();
    for (const auto &val : wpArray) {
        const QJsonObject obj = val.toObject();
        DDSWaypoint wp;
        wp.latitude  = obj[QStringLiteral("lat")].toDouble();
        wp.longitude = obj[QStringLiteral("lon")].toDouble();
        wp.altitude  = static_cast<float>(obj[QStringLiteral("alt")].toDouble(10.0));
        wp.speed     = static_cast<float>(obj[QStringLiteral("speed")].toDouble(-1.0));
        wp.heading   = obj.contains(QStringLiteral("heading"))
                         ? static_cast<float>(obj[QStringLiteral("heading")].toDouble())
                         : NAN;
        wp.hoverTime = static_cast<float>(obj[QStringLiteral("hover")].toDouble(0.0));
        _waypoints.append(wp);
    }

    emit waypointsChanged();
    qInfo() << "[DDSMission] Loaded" << _waypoints.size() << "waypoints from" << filePath;
    return true;
}

// ---- Internal ----

void DDSMissionManager::_setState(State s)
{
    if (_state != s) {
        _state = s;
        emit stateChanged();
    }
}

void DDSMissionManager::_sendCurrentWaypoint()
{
    if (_state != Running || _currentIndex < 0 || _currentIndex >= _waypoints.size()) return;
    if (!_gotoPub || !_gotoPub->isReady()) return;

    const DDSWaypoint &wp = _waypoints[_currentIndex];
    const double wpAltAMSL = _homeAlt + static_cast<double>(wp.altitude);

    const float headingRad = std::isnan(wp.heading)
                               ? NAN
                               : static_cast<float>(qDegreesToRadians(static_cast<double>(wp.heading)));

    _gotoPub->sendGoto(wp.latitude, wp.longitude, wpAltAMSL,
                       _homeLat, _homeLon, _homeAlt,
                       wp.speed, -1.0f, headingRad, -1.0f);
}

void DDSMissionManager::_checkArrival()
{
    if (_state != Running || _currentIndex < 0 || _currentIndex >= _waypoints.size()) return;

    const DDSWaypoint &wp = _waypoints[_currentIndex];
    const double wpAltAMSL = _homeAlt + static_cast<double>(wp.altitude);

    const QGeoCoordinate vehiclePos(_vehicleLat, _vehicleLon);
    const QGeoCoordinate wpPos(wp.latitude, wp.longitude);
    const double hDist = vehiclePos.distanceTo(wpPos);
    const double vDist = fabs(_vehicleAlt - wpAltAMSL);

    _distToWp = hDist;
    emit positionUpdated();

    if (hDist < _arrivalRadiusH && vDist < _arrivalRadiusV) {
        qInfo() << "[DDSMission] Arrived at WP" << _currentIndex
                << "dist=" << hDist << "vDist=" << vDist;
        emit waypointReached(_currentIndex);

        _resendTimer.stop();

        if (wp.hoverTime > 0.0f) {
            _setState(Hovering);
            _hoverTimer.start(static_cast<int>(wp.hoverTime * 1000.0f));
            qInfo() << "[DDSMission] Hovering for" << wp.hoverTime << "s";
        } else {
            _advanceToNext();
        }
    }
}

void DDSMissionManager::_advanceToNext()
{
    _hoverTimer.stop();

    if (_currentIndex + 1 >= _waypoints.size()) {
        _arrivalCheckTimer.stop();
        _resendTimer.stop();
        _setState(Complete);
        emit missionComplete();
        qInfo() << "[DDSMission] Mission complete, executing end action:" << _endAction;
        _executeEndAction();
        return;
    }

    _currentIndex++;
    emit currentWaypointChanged();
    _setState(Running);
    _sendCurrentWaypoint();
    _resendTimer.start();
    qInfo() << "[DDSMission] Advancing to WP" << _currentIndex;
}

void DDSMissionManager::_executeEndAction()
{
    if (!_cmdPub) return;

    switch (_endAction) {
    case HoverInPlace:
        // Already hovering, nothing to do
        qInfo() << "[DDSMission] End action: hover in place";
        break;
    case ReturnToLaunch:
        // MAV_CMD_NAV_RETURN_TO_LAUNCH (20)
        _cmdPub->sendCommand(20, 0, 0, 0, 0, 0, 0, 0);
        qInfo() << "[DDSMission] End action: RTL";
        break;
    case LandInPlace:
        // MAV_CMD_NAV_LAND (21)
        _cmdPub->sendCommand(21, 0, 0, 0, 0, _vehicleLat, _vehicleLon, 0);
        qInfo() << "[DDSMission] End action: land";
        break;
    }
}

#endif // QGC_ENABLE_DDS
