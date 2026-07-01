import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Floating toggle panel for G16 remote controller joystick mapping.
/// Shows connection status and provides an on/off switch.
Rectangle {
    id: root

    width:  _contentColumn.width + ScreenTools.defaultFontPixelWidth * 2
    height: _contentColumn.height + ScreenTools.defaultFontPixelHeight
    radius: ScreenTools.defaultFontPixelWidth / 2
    color:  qgcPal.window
    border.color: qgcPal.groupBorder
    border.width: 1
    opacity: 0.9
    visible: true

    property var _plugin: G16JoystickPlugin

    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    ColumnLayout {
        id: _contentColumn
        anchors.centerIn: parent
        spacing: ScreenTools.defaultFontPixelHeight * 0.3

        // Title row
        RowLayout {
            spacing: ScreenTools.defaultFontPixelWidth * 0.5
            Layout.alignment: Qt.AlignHCenter

            QGCLabel {
                text: qsTr("G16 Remote")
                font.bold: true
                font.pointSize: ScreenTools.smallFontPointSize
            }

            // Connection indicator
            Rectangle {
                width:  ScreenTools.defaultFontPixelWidth * 1.2
                height: width
                radius: width / 2
                color:  _plugin.rcConnected ? "green" : "red"
            }
        }

        // Enable/Disable switch
        RowLayout {
            spacing: ScreenTools.defaultFontPixelWidth * 0.5
            Layout.alignment: Qt.AlignHCenter

            QGCLabel {
                text: qsTr("Mapping")
                font.pointSize: ScreenTools.smallFontPointSize
            }

            Switch {
                id: enableSwitch
                checked: _plugin.enabled
                onToggled: _plugin.enabled = checked
            }
        }

        // Status text
        QGCLabel {
            Layout.alignment: Qt.AlignHCenter
            text: _plugin.statusText
            font.pointSize: ScreenTools.smallFontPointSize * 0.85
            color: qgcPal.colorGrey
            visible: _plugin.enabled
        }
    }
}
