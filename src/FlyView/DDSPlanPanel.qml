import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QGroundControl
import QGroundControl.Controls

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
                    width:  wpList.width
                    height: wpRow.height + ScreenTools.defaultFontPixelHeight / 2
                    color:  index === _currentWp ? qgcPal.buttonHighlight : qgcPal.windowShadeDark
                    radius: ScreenTools.defaultFontPixelHeight / 4

                    RowLayout {
                        id:             wpRow
                        anchors.left:   parent.left
                        anchors.right:  parent.right
                        anchors.margins: ScreenTools.defaultFontPixelHeight / 4
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: ScreenTools.defaultFontPixelWidth

                        QGCLabel {
                            text: qsTr("WP%1").arg(index + 1)
                            font.bold: index === _currentWp
                            color: index === _currentWp ? qgcPal.buttonHighlightText : qgcPal.text
                        }

                        QGCLabel {
                            text: _missionMgr ? qsTr("%1m").arg(_missionMgr.waypointAltitude(index).toFixed(0))
                                              : ""
                            color: index === _currentWp ? qgcPal.buttonHighlightText : qgcPal.text
                            Layout.fillWidth: true
                        }

                        QGCButton {
                            text:       qsTr("Del")
                            enabled:    _state === stateIdle || _state === stateComplete
                            onClicked:  {
                                if (_missionMgr) _missionMgr.removeWaypoint(index)
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
}
