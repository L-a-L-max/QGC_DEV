#pragma once

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtQml/QQmlEngine>

class Vehicle;

/// Bridges the Skydroid G16 remote controller channel data into QGC's
/// virtual joystick pipeline via MAVLink MANUAL_CONTROL.
///
/// On Android, reads channel values from SkydroidRCSDKManager (Java)
/// via JNI at a fixed rate and feeds them into
/// Vehicle::virtualTabletJoystickValue(), which sends MAVLink
/// MANUAL_CONTROL messages to the flight controller.
///
/// On non-Android platforms this class is a no-op stub.
class G16JoystickPlugin : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool    enabled      READ enabled      WRITE setEnabled      NOTIFY enabledChanged)
    Q_PROPERTY(bool    rcConnected  READ rcConnected  NOTIFY rcConnectedChanged)
    Q_PROPERTY(QString statusText   READ statusText   NOTIFY statusTextChanged)
    Q_PROPERTY(QString channelsText READ channelsText NOTIFY channelsTextChanged)

    // Channel mapping (0-based: 0=CH1, 1=CH2, ...)
    Q_PROPERTY(int rollChannel     READ rollChannel     WRITE setRollChannel     NOTIFY channelMappingChanged)
    Q_PROPERTY(int pitchChannel    READ pitchChannel    WRITE setPitchChannel    NOTIFY channelMappingChanged)
    Q_PROPERTY(int throttleChannel READ throttleChannel WRITE setThrottleChannel NOTIFY channelMappingChanged)
    Q_PROPERTY(int yawChannel      READ yawChannel      WRITE setYawChannel      NOTIFY channelMappingChanged)

public:
    explicit G16JoystickPlugin(QObject *parent = nullptr);
    ~G16JoystickPlugin() override;

    static G16JoystickPlugin *instance();
    static G16JoystickPlugin *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool    enabled()      const { return _enabled; }
    bool    rcConnected()  const { return _rcConnected; }
    QString statusText()   const { return _statusText; }
    QString channelsText() const { return _channelsText; }

    int rollChannel()     const { return _rollCh; }
    int pitchChannel()    const { return _pitchCh; }
    int throttleChannel() const { return _throttleCh; }
    int yawChannel()      const { return _yawCh; }

    void setEnabled(bool on);
    void setRollChannel(int ch);
    void setPitchChannel(int ch);
    void setThrottleChannel(int ch);
    void setYawChannel(int ch);

signals:
    void enabledChanged();
    void rcConnectedChanged();
    void statusTextChanged();
    void channelsTextChanged();
    void channelMappingChanged();

private slots:
    void _poll();

private:
    float _normalizeChannel(int rawValue, bool isCentered) const;

    static G16JoystickPlugin *_instance;

    QTimer  _pollTimer;
    bool    _enabled      = false;
    bool    _rcConnected  = false;
    QString _statusText   = QStringLiteral("not initialized");
    QString _channelsText;

    // Channel mapping (0-based indices into 16-channel array)
    // Default: Mode 2 (USA)
    int _rollCh     = 0;   // CH1 = Right stick X
    int _pitchCh    = 1;   // CH2 = Right stick Y
    int _throttleCh = 2;   // CH3 = Left stick Y
    int _yawCh      = 3;   // CH4 = Left stick X

    // Raw value range from G-series remotes
    int _minVal    = 1000;
    int _maxVal    = 2000;
    int _centerVal = 1500;

    static constexpr int kPollIntervalMs = 50;  // 20Hz
};
