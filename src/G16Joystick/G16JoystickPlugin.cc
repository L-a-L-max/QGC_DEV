#include "G16JoystickPlugin.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"

#include <QtCore/QDebug>
#include <cmath>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

Q_LOGGING_CATEGORY(G16JoystickLog, "G16Joystick")

static const char *kJavaClass = "org/mavlink/qgroundcontrol/SkydroidRCSDKManager";

G16JoystickPlugin *G16JoystickPlugin::_instance = nullptr;

G16JoystickPlugin::G16JoystickPlugin(QObject *parent)
    : QObject(parent)
{
    _instance = this;
    _pollTimer.setInterval(kPollIntervalMs);
    connect(&_pollTimer, &QTimer::timeout, this, &G16JoystickPlugin::_poll);
}

G16JoystickPlugin::~G16JoystickPlugin()
{
    _pollTimer.stop();
    if (_instance == this) {
        _instance = nullptr;
    }
}

G16JoystickPlugin *G16JoystickPlugin::instance()
{
    if (!_instance) {
        _instance = new G16JoystickPlugin();
    }
    return _instance;
}

G16JoystickPlugin *G16JoystickPlugin::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

void G16JoystickPlugin::setEnabled(bool on)
{
    if (_enabled == on) return;
    _enabled = on;
    if (_enabled) {
        _pollTimer.start();
        qCInfo(G16JoystickLog) << "G16 joystick mapping enabled";
    } else {
        _pollTimer.stop();
        qCInfo(G16JoystickLog) << "G16 joystick mapping disabled";
    }
    emit enabledChanged();
}

void G16JoystickPlugin::setRollChannel(int ch)
{
    if (_rollCh == ch) return;
    _rollCh = ch;
    emit channelMappingChanged();
}

void G16JoystickPlugin::setPitchChannel(int ch)
{
    if (_pitchCh == ch) return;
    _pitchCh = ch;
    emit channelMappingChanged();
}

void G16JoystickPlugin::setThrottleChannel(int ch)
{
    if (_throttleCh == ch) return;
    _throttleCh = ch;
    emit channelMappingChanged();
}

void G16JoystickPlugin::setYawChannel(int ch)
{
    if (_yawCh == ch) return;
    _yawCh = ch;
    emit channelMappingChanged();
}

float G16JoystickPlugin::_normalizeChannel(int rawValue, bool isCentered) const
{
    if (isCentered) {
        if (rawValue <= _centerVal) {
            float range = static_cast<float>(_centerVal - _minVal);
            if (range < 1.0f) range = 1.0f;
            return static_cast<float>(rawValue - _centerVal) / range;
        } else {
            float range = static_cast<float>(_maxVal - _centerVal);
            if (range < 1.0f) range = 1.0f;
            return static_cast<float>(rawValue - _centerVal) / range;
        }
    } else {
        float range = static_cast<float>(_maxVal - _minVal);
        if (range < 1.0f) range = 1.0f;
        return static_cast<float>(rawValue - _minVal) / range;
    }
}

void G16JoystickPlugin::_poll()
{
#ifdef Q_OS_ANDROID
    // Read connection status
    const bool connected = QJniObject::callStaticMethod<jboolean>(
        kJavaClass, "isRCConnected", "()Z");
    if (_rcConnected != connected) {
        _rcConnected = connected;
        emit rcConnectedChanged();
    }

    // Read status text
    QJniObject statusObj = QJniObject::callStaticObjectMethod(
        kJavaClass, "getStatusText", "()Ljava/lang/String;");
    if (statusObj.isValid()) {
        QString status = statusObj.toString();
        if (_statusText != status) {
            _statusText = status;
            emit statusTextChanged();
        }
    }

    if (!_rcConnected) return;

    // Read channel values text for display
    QJniObject chTextObj = QJniObject::callStaticObjectMethod(
        kJavaClass, "getChannelsText", "()Ljava/lang/String;");
    if (chTextObj.isValid()) {
        QString chText = chTextObj.toString();
        if (_channelsText != chText) {
            _channelsText = chText;
            emit channelsTextChanged();
        }
    }

    // Read individual channel values
    const int rollRaw     = QJniObject::callStaticMethod<jint>(
        kJavaClass, "getChannelValue", "(I)I", _rollCh);
    const int pitchRaw    = QJniObject::callStaticMethod<jint>(
        kJavaClass, "getChannelValue", "(I)I", _pitchCh);
    const int throttleRaw = QJniObject::callStaticMethod<jint>(
        kJavaClass, "getChannelValue", "(I)I", _throttleCh);
    const int yawRaw      = QJniObject::callStaticMethod<jint>(
        kJavaClass, "getChannelValue", "(I)I", _yawCh);

    // Normalize: roll/pitch/yaw -> [-1, +1]; thrust -> [0, 1]
    const float roll   = _normalizeChannel(rollRaw,     true);
    const float pitch  = _normalizeChannel(pitchRaw,    true);
    const float yaw    = _normalizeChannel(yawRaw,      true);
    const float thrust = _normalizeChannel(throttleRaw, false);

    // Suppress sending when all sticks are centered (idle)
    static constexpr float kIdleDeadzone = 0.05f;
    const bool sticksIdle = (fabsf(roll)  < kIdleDeadzone &&
                             fabsf(pitch) < kIdleDeadzone &&
                             fabsf(yaw)   < kIdleDeadzone &&
                             fabsf(thrust - 0.5f) < kIdleDeadzone);
    if (sticksIdle) return;

    // Feed into active vehicle's virtual joystick pipeline (MAVLink)
    Vehicle *vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (vehicle) {
        vehicle->virtualTabletJoystickValue(
            static_cast<double>(roll),
            static_cast<double>(pitch),
            static_cast<double>(yaw),
            static_cast<double>(thrust));
    }

    static int s_logCount = 0;
    if (s_logCount++ % 100 == 0) {
        qCDebug(G16JoystickLog)
            << "CH raw: roll=" << rollRaw << "pitch=" << pitchRaw
            << "thr=" << throttleRaw << "yaw=" << yawRaw
            << "| norm: r=" << roll << "p=" << pitch
            << "y=" << yaw << "t=" << thrust;
    }
#endif // Q_OS_ANDROID
}
