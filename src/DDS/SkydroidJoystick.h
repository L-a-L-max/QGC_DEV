#pragma once

#ifdef QGC_ENABLE_DDS

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVector>

class DDSManualControlPublisher;

/// Bridges the Skydroid RCSDK Android joystick data into QGC's DDS
/// manual-control pipeline.
///
/// On Android, reads channel values from SkydroidRCSDKManager (Java)
/// via JNI at a fixed rate and sends roll/pitch/yaw/thrust commands
/// through DDSManualControlPublisher.
///
/// On non-Android platforms this class does nothing.
class SkydroidJoystick : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool    enabled      READ enabled      WRITE setEnabled      NOTIFY enabledChanged)
    Q_PROPERTY(bool    rcConnected  READ rcConnected   NOTIFY rcConnectedChanged)
    Q_PROPERTY(QString statusText   READ statusText    NOTIFY statusTextChanged)
    Q_PROPERTY(QString channelsText READ channelsText  NOTIFY channelsTextChanged)

public:
    explicit SkydroidJoystick(QObject *parent = nullptr);
    ~SkydroidJoystick() override;

    bool    enabled()      const { return _enabled; }
    bool    rcConnected()  const { return _rcConnected; }
    QString statusText()   const { return _statusText; }
    QString channelsText() const { return _channelsText; }

    void setEnabled(bool on);

    /// Set the DDS publisher to use for sending manual control.
    /// Ownership is NOT transferred.
    void setManualControlPublisher(DDSManualControlPublisher *pub);

    /// Channel index assignments (0-based). Default: Mode 2 (USA hand).
    /// CH0=Roll(right X), CH1=Pitch(right Y), CH2=Throttle(left Y), CH3=Yaw(left X)
    void setChannelMapping(int rollCh, int pitchCh, int throttleCh, int yawCh);

    /// Raw channel value range. Default: 1000..2000 center=1500.
    void setChannelRange(int minVal, int maxVal, int centerVal);

signals:
    void enabledChanged();
    void rcConnectedChanged();
    void statusTextChanged();
    void channelsTextChanged();

private slots:
    void _poll();

private:
    float _normalizeChannel(int rawValue, bool isCentered) const;

    QTimer  _pollTimer;
    bool    _enabled      = false;
    bool    _rcConnected  = false;
    QString _statusText;
    QString _channelsText;

    DDSManualControlPublisher *_manualControlPub = nullptr;

    // Channel mapping (0-based indices into the 16-channel array)
    int _rollCh     = 0;   // CH1 = Right stick X
    int _pitchCh    = 1;   // CH2 = Right stick Y
    int _throttleCh = 2;   // CH3 = Left stick Y
    int _yawCh      = 3;   // CH4 = Left stick X

    // Raw value range
    int _minVal    = 1000;
    int _maxVal    = 2000;
    int _centerVal = 1500;

    static constexpr int kPollIntervalMs = 50;
};

#endif // QGC_ENABLE_DDS
