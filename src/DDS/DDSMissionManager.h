#pragma once

#ifdef QGC_ENABLE_DDS

#include "DDSWaypoint.h"

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>

class DDSGotoPublisher;
class DDSCommandPublisher;

/// Manages DDS-based waypoint missions using GotoSetpoint.
///
/// Maintains a waypoint list and a state machine that publishes
/// goto_setpoint messages sequentially, detecting arrival at each
/// waypoint by comparing vehicle position against the target.
class DDSMissionManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(int currentWaypointIndex READ currentWaypointIndex NOTIFY currentWaypointChanged)
    Q_PROPERTY(int waypointCount READ waypointCount NOTIFY waypointsChanged)
    Q_PROPERTY(EndAction endAction READ endAction WRITE setEndAction NOTIFY endActionChanged)
    Q_PROPERTY(double distanceToWaypoint READ distanceToWaypoint NOTIFY positionUpdated)
    Q_PROPERTY(float defaultAltitude READ defaultAltitude WRITE setDefaultAltitude NOTIFY defaultsChanged)
    Q_PROPERTY(float defaultSpeed READ defaultSpeed WRITE setDefaultSpeed NOTIFY defaultsChanged)

public:
    enum State {
        Idle = 0,
        Running,
        Paused,
        Hovering,
        Complete
    };
    Q_ENUM(State)

    enum EndAction {
        HoverInPlace = 0,
        ReturnToLaunch = 1,
        LandInPlace = 2
    };
    Q_ENUM(EndAction)

    explicit DDSMissionManager(QObject *parent = nullptr);
    ~DDSMissionManager() override;

    void setGotoPublisher(DDSGotoPublisher *pub);
    void setCommandPublisher(DDSCommandPublisher *pub);

    State state() const { return _state; }
    int currentWaypointIndex() const { return _currentIndex; }
    int waypointCount() const { return _waypoints.size(); }
    EndAction endAction() const { return _endAction; }
    void setEndAction(EndAction action);
    double distanceToWaypoint() const { return _distToWp; }
    float defaultAltitude() const { return _defaultAlt; }
    void setDefaultAltitude(float alt) { if (_defaultAlt != alt) { _defaultAlt = alt; emit defaultsChanged(); } }
    float defaultSpeed() const { return _defaultSpeed; }
    void setDefaultSpeed(float spd) { if (_defaultSpeed != spd) { _defaultSpeed = spd; emit defaultsChanged(); } }

    const QVector<DDSWaypoint> &waypoints() const { return _waypoints; }

    // Waypoint management
    Q_INVOKABLE void addWaypoint(double lat, double lon, float alt, float speed, float heading, float hover);
    Q_INVOKABLE void insertWaypoint(int index, double lat, double lon, float alt, float speed, float heading, float hover);
    Q_INVOKABLE void removeWaypoint(int index);
    Q_INVOKABLE void clearWaypoints();
    Q_INVOKABLE void updateWaypoint(int index, double lat, double lon, float alt, float speed, float heading, float hover);

    // QML accessors for waypoint data
    Q_INVOKABLE double waypointLatitude(int index) const;
    Q_INVOKABLE double waypointLongitude(int index) const;
    Q_INVOKABLE float  waypointAltitude(int index) const;
    Q_INVOKABLE float  waypointSpeed(int index) const;

    // Mission control
    Q_INVOKABLE void startMission();
    Q_INVOKABLE void pauseMission();
    Q_INVOKABLE void resumeMission();
    Q_INVOKABLE void stopMission();

    // Save/Load
    Q_INVOKABLE bool saveMission(const QString &filePath);
    Q_INVOKABLE bool loadMission(const QString &filePath);

    /// Called by DDSDataInjector with fresh vehicle position.
    void updateVehiclePosition(double lat, double lon, double altAMSL);

    /// Called by DDSDataInjector with home position.
    void updateHomePosition(double lat, double lon, double altAMSL);

signals:
    void stateChanged();
    void currentWaypointChanged();
    void waypointsChanged();
    void endActionChanged();
    void positionUpdated();
    void waypointReached(int index);
    void missionComplete();
    void missionError(const QString &message);
    void defaultsChanged();

private:
    void _setState(State s);
    void _sendCurrentWaypoint();
    void _advanceToNext();
    void _checkArrival();
    void _executeEndAction();

    DDSGotoPublisher    *_gotoPub = nullptr;
    DDSCommandPublisher *_cmdPub  = nullptr;

    QVector<DDSWaypoint> _waypoints;
    State   _state         = Idle;
    int     _currentIndex  = -1;
    EndAction _endAction   = HoverInPlace;

    // Vehicle state
    double _vehicleLat  = 0.0;
    double _vehicleLon  = 0.0;
    double _vehicleAlt  = 0.0;
    double _homeLat     = 0.0;
    double _homeLon     = 0.0;
    double _homeAlt     = 0.0;
    bool   _homeValid   = false;
    double _distToWp    = 0.0;
    float  _defaultAlt   = 10.0f;
    float  _defaultSpeed = -1.0f;

    // Network resilience
    qint64 _lastPositionUpdateTime = 0;
    static constexpr qint64 _positionTimeoutMs = 10000; // 10s no position update = timeout

    QTimer _arrivalCheckTimer;
    QTimer _hoverTimer;
    QTimer _resendTimer;

    static constexpr double _arrivalRadiusH = 2.0;  // meters
    static constexpr double _arrivalRadiusV = 1.5;  // meters
};

#endif // QGC_ENABLE_DDS
