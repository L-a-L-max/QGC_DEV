import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls

RowLayout {
    id:         control
    spacing:    0

    property bool   showIndicator:        _multipleVehicles
    property var    _activeVehicle:       QGroundControl.multiVehicleManager.activeVehicle
    property bool   _multipleVehicles:    QGroundControl.multiVehicleManager.vehicles.count > 1
    

    RowLayout {
        Layout.fillWidth: true

        QGCColoredImage {
            width:      ScreenTools.defaultFontPixelWidth * 4
            height:     ScreenTools.defaultFontPixelHeight * 1.33
            fillMode:   Image.PreserveAspectFit
            mipmap:     true
            color:      qgcPal.text
            source:     "/InstrumentValueIcons/airplane.svg"
        }

        QGCLabel {
            text:               _activeVehicle ? (_activeVehicle.customName.length > 0 ? _activeVehicle.customName : qsTr("Vehicle") + " " + _activeVehicle.id) : qsTr("N/A")
            font.pointSize:     ScreenTools.mediumFontPointSize
            Layout.alignment:   Qt.AlignCenter

            MouseArea {
                anchors.fill:   parent
                onClicked:      mainWindow.showIndicatorDrawer(vehicleSelectorDrawer, control)
            }
        }
    }

    Component {
        id: vehicleSelectorDrawer

        ToolIndicatorPage {
            showExpand: true

            contentComponent: Component {
                ColumnLayout {
                    spacing: ScreenTools.defaultFontPixelWidth / 2

                    Repeater {
                        model: QGroundControl.multiVehicleManager.vehicles

                        QGCButton {
                            text:               object.customName.length > 0 ? object.customName : qsTr("Vehicle") + " " + object.id
                            Layout.fillWidth:   true

                            onClicked: {
                                QGroundControl.multiVehicleManager.activeVehicle = object
                                mainWindow.closeIndicatorDrawer()
                            }
                        }
                    }
                }
            }

            expandedComponent: Component {
                SettingsGroupLayout {
                    Layout.fillWidth: true

                    FactCheckBoxSlider {
                        Layout.fillWidth:   true
                        text:               qsTr("Enable Multi-Vehicle Panel")
                        fact:               _enableMultiVehiclePanel
                        visible:            _enableMultiVehiclePanel.visible

                        property Fact _enableMultiVehiclePanel: QGroundControl.settingsManager.appSettings.enableMultiVehiclePanel
                    }
                }
            }
        }
    }

}
