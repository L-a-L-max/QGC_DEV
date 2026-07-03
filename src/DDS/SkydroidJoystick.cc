#ifdef QGC_ENABLE_DDS

#include "SkydroidJoystick.h"
#include "DDSManualControlPublisher.h"

#include <QtCore/QDebug>
#include <cmath>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

Q_LOGGING_CATEGORY(SkydroidJoystickLog, "DDS.SkydroidJoystick")

static const char *kJavaClass = "org/mavlink/qgroundcontrol/SkydroidRCSDKManager";

SkydroidJoystick::SkydroidJoystick(QObject *parent)
    : QObject(parent)
{
    _pollTimer.setInterval(kPollIntervalMs);
    connect(&_pollTimer, &QTimer::timeout, this, &SkydroidJoystick::_poll);
}

SkydroidJoystick::~SkydroidJoystick()
{
    _pollTimer.stop();
}

void SkydroidJoystick::setEnabled(bool on)
{
    if (_enabled == on) return;
    _enabled = on;
    if (_enabled) {
        _pollTimer.start();
        qCInfo(SkydroidJoystickLog) << "Skydroid joystick polling enabled";
    } else {
        _pollTimer.stop();
        qCInfo(SkydroidJoystickLog) << "Skydroid joystick polling disabled";
    }
    emit enabledChanged();
}

void SkydroidJoystick::setManualControlPublisher(DDSManualControlPublisher *pub)
{
    _manualControlPub = pub;
}

void SkydroidJoystick::setChannelMapping(int rollCh, int pitchCh, int throttleCh, int yawCh)
{
    _rollCh     = rollCh;
    _pitchCh    = pitchCh;
    _throttleCh = throttleCh;
    _yawCh      = yawCh;
}

void SkydroidJoystick::setChannelRange(int minVal, int maxVal, int centerVal)
{
    _minVal    = minVal;
    _maxVal    = maxVal;
    _centerVal = centerVal;
}

float SkydroidJoystick::_normalizeChannel(int rawValue, bool isCentered) const
{
    // Clamp raw value to valid range; out-of-range values (e.g. 0 when
    // the controller is disconnected) would produce extreme outputs.
    rawValue = qBound(_minVal, rawValue, _maxVal);

    if (isCentered) {
        // Map [min..center..max] → [-1..0..+1]
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
        // Map [min..max] → [0..1] (for throttle)
        float range = static_cast<float>(_maxVal - _minVal);
        if (range < 1.0f) range = 1.0f;
        return static_cast<float>(rawValue - _minVal) / range;
    }
}

void SkydroidJoystick::_poll()
{
#ifdef Q_OS_ANDROID
    // Read connection status
    const bool connected = QJniObject::callStaticMethod<jboolean>(kJavaClass, "isRCConnected", "()Z");
    if (_rcConnected != connected) {
        _rcConnected = connected;
        emit rcConnectedChanged();
    }

    // Read status text
    QJniObject statusObj = QJniObject::callStaticObjectMethod(kJavaClass, "getStatusText", "()Ljava/lang/String;");
    if (statusObj.isValid()) {
        QString status = statusObj.toString();
        if (_statusText != status) {
            _statusText = status;
            emit statusTextChanged();
        }
    }

    if (!_rcConnected) {
        // RC disconnected: send center/hover values so PX4 does not
        // trigger RC-loss failsafe (RTL).  throttle=0.5 maps to PX4
        // throttle=0 (hover for multirotor).
        if (_manualControlPub && _manualControlPub->isReady()) {
            _manualControlPub->sendManualControl(0.0f, 0.0f, 0.0f, 0.5f);
        }
        return;
    }

    // Read channel values text for display
    QJniObject chTextObj = QJniObject::callStaticObjectMethod(kJavaClass, "getChannelsText", "()Ljava/lang/String;");
    if (chTextObj.isValid()) {
        QString chText = chTextObj.toString();
        if (_channelsText != chText) {
            _channelsText = chText;
            emit channelsTextChanged();
        }
    }

    // Read individual channel values for control
    const int rollRaw     = QJniObject::callStaticMethod<jint>(kJavaClass, "getChannelValue", "(I)I", _rollCh);
    const int pitchRaw    = QJniObject::callStaticMethod<jint>(kJavaClass, "getChannelValue", "(I)I", _pitchCh);
    const int throttleRaw = QJniObject::callStaticMethod<jint>(kJavaClass, "getChannelValue", "(I)I", _throttleCh);
    const int yawRaw      = QJniObject::callStaticMethod<jint>(kJavaClass, "getChannelValue", "(I)I", _yawCh);

    // Normalize: roll/pitch/yaw → [-1, +1]; thrust → [0, 1]
    const float roll   = _normalizeChannel(rollRaw,     true);
    const float pitch  = _normalizeChannel(pitchRaw,    true);
    const float yaw    = _normalizeChannel(yawRaw,      true);
    const float thrust = _normalizeChannel(throttleRaw, false);

    // Send via DDS (including center/idle values so PX4 keeps
    // receiving heartbeats and does not trigger RC-loss → RTL)
    if (_manualControlPub && _manualControlPub->isReady()) {
        _manualControlPub->sendManualControl(roll, pitch, yaw, thrust);
    }

    static int s_logCount = 0;
    if (s_logCount++ % 50 == 0) {
        qCDebug(SkydroidJoystickLog)
            << "CH raw: roll=" << rollRaw << "pitch=" << pitchRaw
            << "thr=" << throttleRaw << "yaw=" << yawRaw
            << "| norm: r=" << roll << "p=" << pitch
            << "y=" << yaw << "t=" << thrust;
    }
#else
    Q_UNUSED(this)
#endif // Q_OS_ANDROID
}

#endif // QGC_ENABLE_DDS
