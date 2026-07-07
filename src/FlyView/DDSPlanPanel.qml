import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QGroundControl.VehicleSetup

Rectangle {
    id:     root
    width:  ScreenTools.defaultFontPixelWidth * 35
    color:  qgcPal.window
    radius: ScreenTools.defaultFontPixelHeight / 2
    clip:   true

    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    property var _missionMgr:   _activeVehicle ? _activeVehicle.ddsMissionMgr : null
    property int _wpCount:      _missionMgr ? _missionMgr.waypointCount : 0
    property int _currentWp:    _missionMgr ? _missionMgr.currentWaypointIndex : -1
    property int _state:        _missionMgr ? _missionMgr.state : 0

    property int _editingIndex: -1  // Which waypoint is being edited (-1 = none)

    signal addWaypointFromMap()
    signal closePanel()

    QGCPalette { id: qgcPal }

    // State constants matching DDSMissionManager::State enum
    readonly property int stateIdle:     0
    readonly property int stateRunning:  1
    readonly property int statePaused:   2
    readonly property int stateHovering: 3
    readonly property int stateComplete: 4

    function stateText() {
        switch (_state) {
        case stateIdle:     return qsTr("Idle")
        case stateRunning:  return qsTr("Running")
        case statePaused:   return qsTr("Paused")
        case stateHovering: return qsTr("Hovering")
        case stateComplete: return qsTr("Complete")
        default:            return qsTr("Unknown")
        }
    }

    DeadMouseArea { anchors.fill: parent }

    ColumnLayout {
        anchors.fill:       parent
        anchors.margins:    ScreenTools.defaultFontPixelHeight / 2
        spacing:            ScreenTools.defaultFontPixelHeight / 2

        // Header
        RowLayout {
            Layout.fillWidth: true
            QGCLabel {
                text:               qsTr("DDS Mission")
                font.pointSize:     ScreenTools.mediumFontPointSize
                font.bold:          true
                Layout.fillWidth:   true
            }
            QGCButton {
                text:       qsTr("X")
                onClicked:  root.closePanel()
            }
        }

        // Status bar
        RowLayout {
            Layout.fillWidth: true
            QGCLabel { text: qsTr("Status:"); font.bold: true }
            QGCLabel { text: stateText() }
            Item { Layout.fillWidth: true }
            QGCLabel {
                text: _missionMgr ? qsTr("Dist: %1m").arg(_missionMgr.distanceToWaypoint.toFixed(1)) : ""
                visible: _state === stateRunning || _state === stateHovering
            }
        }

        // Waypoint list
        Rectangle {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            color:             qgcPal.windowShade
            radius:            ScreenTools.defaultFontPixelHeight / 4
            clip:              true

            ListView {
                id:             wpList
                anchors.fill:   parent
                anchors.margins: ScreenTools.defaultFontPixelHeight / 4
                model:          _wpCount
                spacing:        2
                delegate: Rectangle {
                    id: wpDelegate
                    width:  wpList.width
                    height: wpCol.height + ScreenTools.defaultFontPixelHeight / 4
                    color:  index === _currentWp ? qgcPal.buttonHighlight : qgcPal.windowShadeDark
                    radius: ScreenTools.defaultFontPixelHeight / 4

                    property bool isEditing: _editingIndex === index

                    Column {
                        id:             wpCol
                        anchors.left:   parent.left
                        anchors.right:  parent.right
                        anchors.margins: ScreenTools.defaultFontPixelHeight / 4
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        RowLayout {
                            width: parent.width
                            spacing: ScreenTools.defaultFontPixelWidth

                            QGCLabel {
                                text: qsTr("WP%1").arg(index + 1)
                                font.bold: true
                                color: index === _currentWp ? qgcPal.buttonHighlightText : qgcPal.text
                            }

                            QGCLabel {
                                text: _missionMgr ? qsTr("%1m  %2m/s").arg(
                                    _missionMgr.waypointAltitude(index).toFixed(0)).arg(
                                    _missionMgr.waypointSpeed(index) < 0 ? "auto" :
                                    _missionMgr.waypointSpeed(index).toFixed(1))
                                    : ""
                                color: index === _currentWp ? qgcPal.buttonHighlightText : qgcPal.text
                                Layout.fillWidth: true
                            }

                            QGCButton {
                                text:       qsTr("Ins")
                                onClicked:  {
                                    if (_missionMgr) {
                                        var alt = parseFloat(altField.text) || 10.0
                                        var spd = parseFloat(speedField.text) || -1.0
                                        _missionMgr.insertWaypoint(index + 1,
                                            _missionMgr.waypointLatitude(index),
                                            _missionMgr.waypointLongitude(index),
                                            alt, spd, NaN, 0.0)
                                    }
                                }
                            }
                            QGCButton {
                                text:       qsTr("Del")
                                enabled:    _state === stateIdle || _state === stateComplete || _state === statePaused
                                onClicked:  {
                                    if (_missionMgr) _missionMgr.removeWaypoint(index)
                                }
                            }
                        }

                        // Tap coordinate label to open edit mode
                        QGCLabel {
                            text: _missionMgr ? qsTr("  %1, %2").arg(
                                _missionMgr.waypointLatitude(index).toFixed(6)).arg(
                                _missionMgr.waypointLongitude(index).toFixed(6))
                                : ""
                            font.pointSize: ScreenTools.smallFontPointSize
                            color: index === _currentWp ? qgcPal.buttonHighlightText : qgcPal.colorGrey
                            visible: !wpDelegate.isEditing

                            MouseArea {
                                anchors.fill: parent
                                onClicked: _editingIndex = index
                            }
                        }

                        // Editable coordinate fields (shown when tapped)
                        GridLayout {
                            visible: wpDelegate.isEditing
                            columns: 2
                            columnSpacing: 4
                            rowSpacing: 2
                            width: parent.width

                            QGCLabel { text: qsTr("Lat:"); font.pointSize: ScreenTools.smallFontPointSize }
                            TextField {
                                id: editLatField
                                Layout.fillWidth: true
                                font.pointSize: ScreenTools.smallFontPointSize
                                text: _missionMgr ? _missionMgr.waypointLatitude(index).toFixed(7) : ""
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                selectByMouse: true
                                background: Rectangle { color: qgcPal.textField; border.color: qgcPal.groupBorder; radius: 2 }
                                color: qgcPal.text
                            }
                            QGCLabel { text: qsTr("Lon:"); font.pointSize: ScreenTools.smallFontPointSize }
                            TextField {
                                id: editLonField
                                Layout.fillWidth: true
                                font.pointSize: ScreenTools.smallFontPointSize
                                text: _missionMgr ? _missionMgr.waypointLongitude(index).toFixed(7) : ""
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                selectByMouse: true
                                background: Rectangle { color: qgcPal.textField; border.color: qgcPal.groupBorder; radius: 2 }
                                color: qgcPal.text
                            }
                            QGCLabel { text: qsTr("Alt:"); font.pointSize: ScreenTools.smallFontPointSize }
                            TextField {
                                id: editAltField
                                Layout.fillWidth: true
                                font.pointSize: ScreenTools.smallFontPointSize
                                text: _missionMgr ? _missionMgr.waypointAltitude(index).toFixed(1) : ""
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                selectByMouse: true
                                background: Rectangle { color: qgcPal.textField; border.color: qgcPal.groupBorder; radius: 2 }
                                color: qgcPal.text
                            }
                        }

                        // Save/Cancel buttons for edit mode
                        RowLayout {
                            visible: wpDelegate.isEditing
                            width: parent.width
                            spacing: ScreenTools.defaultFontPixelWidth

                            Item { Layout.fillWidth: true }
                            QGCButton {
                                text: qsTr("Save")
                                onClicked: {
                                    if (_missionMgr) {
                                        var lat = parseFloat(editLatField.text)
                                        var lon = parseFloat(editLonField.text)
                                        var alt = parseFloat(editAltField.text)
                                        var spd = _missionMgr.waypointSpeed(index)
                                        if (!isNaN(lat) && !isNaN(lon) && !isNaN(alt)) {
                                            _missionMgr.updateWaypoint(index, lat, lon, alt, spd, NaN, 0.0)
                                        }
                                    }
                                    _editingIndex = -1
                                }
                            }
                            QGCButton {
                                text: qsTr("Cancel")
                                onClicked: _editingIndex = -1
                            }
                        }
                    }
                }

                QGCLabel {
                    anchors.centerIn: parent
                    text:             qsTr("No waypoints\nClick map to add")
                    visible:          _wpCount === 0
                    horizontalAlignment: Text.AlignHCenter
                    color:            qgcPal.text
                }
            }
        }

        // Add waypoint at vehicle position
        QGCButton {
            Layout.fillWidth: true
            text:             qsTr("Add WP at Vehicle Pos")
            enabled:          _missionMgr && _activeVehicle && _activeVehicle.coordinate.isValid &&
                              (_state === stateIdle || _state === stateComplete)
            onClicked: {
                if (_missionMgr && _activeVehicle) {
                    var coord = _activeVehicle.coordinate
                    var alt = parseFloat(altField.text) || 10.0
                    var spd = parseFloat(speedField.text) || -1.0
                    _missionMgr.addWaypoint(coord.latitude, coord.longitude, alt, spd, NaN, 0.0)
                }
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text:             qsTr("Or right-click map to add waypoint")
            font.pointSize:   ScreenTools.smallFontPointSize
            horizontalAlignment: Text.AlignHCenter
            color:            qgcPal.text
        }

        // Waypoint settings
        GridLayout {
            Layout.fillWidth: true
            columns:          2
            columnSpacing:    ScreenTools.defaultFontPixelWidth
            rowSpacing:       ScreenTools.defaultFontPixelHeight / 4

            QGCLabel { text: qsTr("Altitude (m):") }
            QGCTextField {
                id:                 altField
                Layout.fillWidth:   true
                text:               _missionMgr ? _missionMgr.defaultAltitude.toString() : "10"
                inputMethodHints:   Qt.ImhFormattedNumbersOnly
                onEditingFinished: {
                    if (_missionMgr) _missionMgr.defaultAltitude = parseFloat(text) || 10.0
                }
            }

            QGCLabel { text: qsTr("Speed (m/s):") }
            QGCTextField {
                id:                 speedField
                Layout.fillWidth:   true
                text:               _missionMgr ? _missionMgr.defaultSpeed.toString() : "-1"
                inputMethodHints:   Qt.ImhFormattedNumbersOnly
                onEditingFinished: {
                    if (_missionMgr) _missionMgr.defaultSpeed = parseFloat(text) || -1.0
                }
            }

            QGCLabel { text: qsTr("End Action:") }
            QGCComboBox {
                id:                 endActionCombo
                Layout.fillWidth:   true
                model:              [qsTr("Hover"), qsTr("RTL"), qsTr("Land")]
                currentIndex:       _missionMgr ? _missionMgr.endAction : 0
                onActivated: function(idx) {
                    if (_missionMgr) _missionMgr.endAction = idx
                }
            }
        }

        // Mission control buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                Layout.fillWidth: true
                text:             qsTr("Start")
                enabled:          _missionMgr && _wpCount > 0 &&
                                  (_state === stateIdle || _state === stateComplete)
                onClicked:        { if (_missionMgr) _missionMgr.startMission() }
            }

            QGCButton {
                Layout.fillWidth: true
                text:             _state === statePaused ? qsTr("Resume") : qsTr("Pause")
                enabled:          _missionMgr &&
                                  (_state === stateRunning || _state === stateHovering || _state === statePaused)
                onClicked: {
                    if (!_missionMgr) return
                    if (_state === statePaused)
                        _missionMgr.resumeMission()
                    else
                        _missionMgr.pauseMission()
                }
            }

            QGCButton {
                Layout.fillWidth: true
                text:             qsTr("Stop")
                enabled:          _missionMgr && _state !== stateIdle
                onClicked:        { if (_missionMgr) _missionMgr.stopMission() }
            }
        }

        // Save/Load row
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            QGCButton {
                Layout.fillWidth: true
                text:             qsTr("Save")
                enabled:          _missionMgr && _wpCount > 0
                onClicked:        saveDialog.open()
            }

            QGCButton {
                Layout.fillWidth: true
                text:             qsTr("Load")
                enabled:          _missionMgr && (_state === stateIdle || _state === stateComplete)
                onClicked:        loadDialog.open()
            }

            QGCButton {
                Layout.fillWidth: true
                text:             qsTr("Clear")
                enabled:          _missionMgr && _wpCount > 0 &&
                                  (_state === stateIdle || _state === stateComplete)
                onClicked:        { if (_missionMgr) _missionMgr.clearWaypoints() }
            }
        }

        // Joystick settings
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: qgcPal.groupBorder
        }

        QGCLabel {
            text: qsTr("Joystick Settings")
            font.bold: true
        }

        // USB Joystick DDS toggle — release control by disabling
        RowLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelWidth

            property var _appSettings: QGroundControl.settingsManager.appSettings

            QGCCheckBox {
                id:      usbJoystickDdsCheck
                text:    qsTr("USB Joystick → DDS")
                checked: parent._appSettings.ddsUsbJoystickEnabled.rawValue
                onClicked: parent._appSettings.ddsUsbJoystickEnabled.rawValue = checked
            }
            QGCLabel {
                text:  usbJoystickDdsCheck.checked ? qsTr("Sending") : qsTr("Released")
                color: usbJoystickDdsCheck.checked ? qgcPal.colorGreen : qgcPal.colorOrange
                font.bold: true
            }
        }

        // Joystick calibration (works without parameter page)
        QGCButton {
            Layout.fillWidth: true
            text:             qsTr("Joystick Calibration...")
            enabled:          joystickManager.activeJoystick
            onClicked:        joystickCalibrationDialog.open()
        }

        GridLayout {
            Layout.fillWidth: true
            columns:          3
            columnSpacing:    ScreenTools.defaultFontPixelWidth / 2
            rowSpacing:       ScreenTools.defaultFontPixelHeight / 4

            property var _appSettings: QGroundControl.settingsManager.appSettings

            QGCLabel { text: qsTr("DDS Priority") }
            QGCComboBox {
                id:                 dataSourceCombo
                Layout.fillWidth:   true
                model:              parent._appSettings.ddsDataSource.enumStrings
                currentIndex:       parent._appSettings.ddsDataSource.enumIndex
                onActivated: function(index) {
                    parent._appSettings.ddsDataSource.value = parent._appSettings.ddsDataSource.enumValues[index]
                }
            }
            QGCLabel {
                text: "data_source=" + parent._appSettings.ddsDataSource.rawValue
                Layout.minimumWidth: ScreenTools.defaultFontPixelWidth * 6
            }

            QGCLabel { text: qsTr("Max Speed") }
            Slider {
                id:                 maxSpeedSlider
                Layout.fillWidth:   true
                from:               1
                to:                 20
                stepSize:           1
                value:              parent._appSettings.virtualJoystickMaxSpeed.rawValue
                onMoved:            parent._appSettings.virtualJoystickMaxSpeed.rawValue = value
            }
            QGCLabel {
                text: qsTr("%1 m/s").arg(maxSpeedSlider.value.toFixed(0))
                Layout.minimumWidth: ScreenTools.defaultFontPixelWidth * 6
            }

            QGCLabel { text: qsTr("Deadzone") }
            Slider {
                id:                 deadzoneSlider
                Layout.fillWidth:   true
                from:               0
                to:                 0.3
                stepSize:           0.01
                value:              parent._appSettings.virtualJoystickDeadzone.rawValue
                onMoved:            parent._appSettings.virtualJoystickDeadzone.rawValue = value
            }
            QGCLabel {
                text: deadzoneSlider.value.toFixed(2)
                Layout.minimumWidth: ScreenTools.defaultFontPixelWidth * 6
            }

            QGCLabel { text: qsTr("Expo") }
            Slider {
                id:                 expoSlider
                Layout.fillWidth:   true
                from:               0
                to:                 0.8
                stepSize:           0.05
                value:              parent._appSettings.virtualJoystickExpo.rawValue
                onMoved:            parent._appSettings.virtualJoystickExpo.rawValue = value
            }
            QGCLabel {
                text: expoSlider.value.toFixed(2)
                Layout.minimumWidth: ScreenTools.defaultFontPixelWidth * 6
            }
        }
    }

    // Properties used by FlyView to add waypoints from map clicks
    property real defaultAltitude: parseFloat(altField.text) || 10.0
    property real defaultSpeed:    parseFloat(speedField.text) || -1.0

    FileDialog {
        id:             saveDialog
        title:          qsTr("Save DDS Mission")
        nameFilters:    ["DDS Mission (*.ddsmission)", "All files (*)"]
        fileMode:       FileDialog.SaveFile
        defaultSuffix:  "ddsmission"
        onAccepted: {
            if (_missionMgr) {
                var path = selectedFile.toString().replace("file://", "")
                _missionMgr.saveMission(path)
            }
        }
    }

    FileDialog {
        id:             loadDialog
        title:          qsTr("Load DDS Mission")
        nameFilters:    ["DDS Mission (*.ddsmission)", "All files (*)"]
        fileMode:       FileDialog.OpenFile
        onAccepted: {
            if (_missionMgr) {
                var path = selectedFile.toString().replace("file://", "")
                _missionMgr.loadMission(path)
            }
        }
    }

    // Joystick calibration dialog — works without the parameter-gated Vehicle Setup page
    Popup {
        id:             joystickCalibrationDialog
        parent:         Overlay.overlay
        anchors.centerIn: parent
        width:          Math.min(parent.width * 0.9, ScreenTools.defaultFontPixelWidth * 80)
        height:         Math.min(parent.height * 0.85, ScreenTools.defaultFontPixelHeight * 45)
        modal:          true
        closePolicy:    Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding:        ScreenTools.defaultFontPixelHeight

        background: Rectangle {
            color:  qgcPal.window
            radius: ScreenTools.defaultFontPixelHeight / 2
            border.color: qgcPal.groupBorder
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: ScreenTools.defaultFontPixelHeight / 2

            RowLayout {
                Layout.fillWidth: true
                QGCLabel {
                    text:               qsTr("Joystick Calibration")
                    font.pointSize:     ScreenTools.mediumFontPointSize
                    font.bold:          true
                    Layout.fillWidth:   true
                }
                QGCButton {
                    text:       qsTr("Close")
                    onClicked:  joystickCalibrationDialog.close()
                }
            }

            Loader {
                Layout.fillWidth:  true
                Layout.fillHeight: true
                active:            joystickCalibrationDialog.opened && joystickManager.activeJoystick
                sourceComponent:   joystickCalibrationComponent
            }

            QGCLabel {
                visible:            !joystickManager.activeJoystick
                text:               qsTr("No joystick detected. Connect a USB joystick and try again.")
                Layout.fillWidth:   true
                wrapMode:           Text.WordWrap
            }
        }
    }

    Component {
        id: joystickCalibrationComponent

        ColumnLayout {
            spacing: ScreenTools.defaultFontPixelHeight / 2

            property var _activeJoystick: joystickManager.activeJoystick

            RowLayout {
                spacing: ScreenTools.defaultFontPixelWidth

                QGCLabel { text: qsTr("Joystick:"); font.bold: true }
                QGCLabel { text: _activeJoystick ? _activeJoystick.name : "" }
                Item { Layout.fillWidth: true }
                QGCLabel {
                    text: _activeJoystick && _activeJoystick.settings.calibrated.rawValue
                          ? qsTr("Calibrated")
                          : qsTr("Needs Calibration")
                    font.bold: true
                    color: _activeJoystick && _activeJoystick.settings.calibrated.rawValue
                           ? qgcPal.colorGreen : qgcPal.colorOrange
                }
            }

            RemoteControlCalibration {
                id: remoteControlCalibration
                Layout.fillWidth:  true
                Layout.fillHeight: true

                controller: JoystickConfigController {
                    joystick:     joystickManager.activeJoystick
                    statusText:   remoteControlCalibration.statusText
                    cancelButton: remoteControlCalibration.cancelButton
                    nextButton:   remoteControlCalibration.nextButton
                    joystickMode: true
                }

                useDeadband: controller && controller.joystick && controller.joystick.settings.useDeadband.rawValue

                Component.onCompleted: controller.start()
            }
        }
    }
}
