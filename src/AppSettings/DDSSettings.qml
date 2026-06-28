import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    spacing: _rowSpacing

    function saveSettings() {
        // No need — properties are bound directly
    }

    readonly property bool _configValid: subEditConfig !== null && subEditConfig !== undefined

    readonly property var _profileDefs: [
        { label: "PX4 v1.17 (Full)",   mapping: "",           idl: "v1"        },
        { label: "PX4 SITL v1.17",     mapping: "",           idl: "v1"        },
        { label: "PX4 SITL v1.16",     mapping: "",           idl: "px4_v116"  },
        { label: "CUAV X7+ (v1.16)",   mapping: "cuav_x7pro", idl: "px4_v116"  },
        { label: "Custom...",           mapping: "_custom",    idl: ""          }
    ]

    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Domain ID") }
        QGCTextField {
            id:                     domainField
            text:                   _configValid ? subEditConfig.domainId.toString() : "0"
            focus:                  true
            Layout.preferredWidth:  _secondColumnWidth
            inputMethodHints:       Qt.ImhFormattedNumbersOnly
            onTextChanged:          { if (_configValid) subEditConfig.domainId = parseInt(domainField.text) || 0 }
        }
    }

    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("DDS Profile") }
        QGCComboBox {
            id:                     profileCombo
            Layout.preferredWidth:  _secondColumnWidth
            model: {
                var labels = []
                for (var i = 0; i < _profileDefs.length; i++)
                    labels.push(_profileDefs[i].label)
                return labels
            }
            currentIndex: {
                if (!_configValid) return 0
                for (var i = 0; i < _profileDefs.length - 1; i++) {
                    if (_profileDefs[i].mapping === subEditConfig.vendorMapping &&
                        _profileDefs[i].idl === subEditConfig.idlVersion)
                        return i
                }
                if (subEditConfig.vendorMapping.length > 0 &&
                    subEditConfig.vendorMapping !== "cuav_x7pro")
                    return _profileDefs.length - 1
                return 0
            }
            onActivated: function(idx) {
                if (!_configValid) return
                var def = _profileDefs[idx]
                if (def.mapping === "_custom") {
                    customField.visible = true
                } else {
                    customField.visible = false
                    subEditConfig.vendorMapping = def.mapping
                    subEditConfig.idlVersion    = def.idl
                }
            }
        }
    }

    QGCTextField {
        id:                     customField
        Layout.fillWidth:       true
        visible:                profileCombo.currentIndex === (_profileDefs.length - 1)
        text:                   (_configValid && subEditConfig.vendorMapping !== "" &&
                                 subEditConfig.vendorMapping !== "cuav_x7pro")
                                    ? subEditConfig.vendorMapping : ""
        placeholderText:        qsTr("JSON file name (without .json)")
        onEditingFinished:      { if (_configValid) subEditConfig.vendorMapping = text }
    }

    QGCLabel {
        Layout.fillWidth:       true
        visible:                customField.visible
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        color:                  qgcPal.text
        text:                   qsTr("Place custom .json files in:\n~/.config/QGroundControl.org/dds_mappings/")
    }

    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Peer Address") }
        QGCTextField {
            id:                     peerField
            text:                   _configValid ? (subEditConfig.peerAddress || "") : ""
            Layout.preferredWidth:  _secondColumnWidth
            placeholderText:        qsTr("e.g. 192.168.1.100")
            onTextChanged:          { if (_configValid) subEditConfig.peerAddress = peerField.text }
        }
    }

    QGCLabel {
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        color:                  qgcPal.text
        text:                   qsTr("IP address of the machine running PX4 DDS Agent. "
                                     + "Required when multicast discovery does not work "
                                     + "(e.g. across routers, Wi-Fi to Ethernet, Android). "
                                     + "Separate multiple addresses with commas.")
    }

    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Namespace Prefix") }
        QGCTextField {
            id:                     nsField
            text:                   _configValid ? (subEditConfig.namespacePrefix || "") : ""
            Layout.preferredWidth:  _secondColumnWidth
            placeholderText:        qsTr("e.g. /drone1")
            onTextChanged:          { if (_configValid) subEditConfig.namespacePrefix = nsField.text }
        }
    }

    RowLayout {
        spacing: _colSpacing

        QGCCheckBoxSlider {
            text:       qsTr("Auto-Discover Topics")
            checked:    _configValid ? subEditConfig.autoDiscover : false
            onClicked:  { if (_configValid) subEditConfig.autoDiscover = checked }
        }
    }

    RowLayout {
        spacing: _colSpacing

        QGCCheckBoxSlider {
            text:       qsTr("Skydroid Joystick (G16/G20)")
            checked:    _configValid ? subEditConfig.skydroidJoystick : false
            onClicked:  { if (_configValid) subEditConfig.skydroidJoystick = checked }
        }
    }

    QGCLabel {
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        visible:                _configValid && subEditConfig.skydroidJoystick
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        color:                  qgcPal.text
        text:                   qsTr("Enable to use Skydroid G-series remote controller joysticks "
                                     + "to control the drone via DDS. Requires RCSDK AAR integrated "
                                     + "into the APK build. Channel mapping: "
                                     + "CH1=Roll, CH2=Pitch, CH3=Throttle, CH4=Yaw (Mode 2).")
    }

    QGCLabel {
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("DDS link connects to PX4 flight controllers via CycloneDDS. "
                                     + "Select a profile matching your PX4 firmware version. "
                                     + "Domain ID must match the PX4 DDS domain (default 0).\n\n"
                                     + "PX4 v1.17 (Full) — All 24 PX4 v1.17 output topics\n"
                                     + "PX4 SITL v1.17 — Software-in-the-loop simulation (latest)\n"
                                     + "PX4 SITL v1.16 — Software-in-the-loop simulation (v1.16)\n"
                                     + "CUAV X7+ (v1.16) — CUAV X7+ Pro hardware with v1.16 firmware\n\n"
                                     + "If topics show UNMATCHED, fill in the Peer Address field with the IP "
                                     + "of the machine running the PX4 DDS Agent to enable unicast discovery.")
    }
}
