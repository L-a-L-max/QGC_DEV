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

    readonly property var _profileDefs: [
        { label: "PX4 SITL v1.17",   mapping: "",           idl: "v1"        },
        { label: "PX4 SITL v1.16",   mapping: "",           idl: "px4_v116"  },
        { label: "CUAV X7+ (v1.16)", mapping: "cuav_x7pro", idl: "px4_v116"  },
        { label: "Custom...",         mapping: "_custom",    idl: ""          }
    ]

    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Domain ID") }
        QGCTextField {
            id:                     domainField
            text:                   subEditConfig.domainId.toString()
            focus:                  true
            Layout.preferredWidth:  _secondColumnWidth
            inputMethodHints:       Qt.ImhFormattedNumbersOnly
            onTextChanged:          subEditConfig.domainId = parseInt(domainField.text) || 0
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
        text:                   (subEditConfig.vendorMapping !== "" &&
                                 subEditConfig.vendorMapping !== "cuav_x7pro")
                                    ? subEditConfig.vendorMapping : ""
        placeholderText:        qsTr("JSON file name (without .json)")
        onEditingFinished:      subEditConfig.vendorMapping = text
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

        QGCLabel { text: qsTr("Namespace Prefix") }
        QGCTextField {
            id:                     nsField
            text:                   subEditConfig.namespacePrefix
            Layout.preferredWidth:  _secondColumnWidth
            placeholderText:        qsTr("e.g. /drone1")
            onTextChanged:          subEditConfig.namespacePrefix = nsField.text
        }
    }

    RowLayout {
        spacing: _colSpacing

        QGCCheckBoxSlider {
            text:       qsTr("Auto-Discover Topics")
            checked:    subEditConfig.autoDiscover
            onClicked:  subEditConfig.autoDiscover = checked
        }
    }

    QGCLabel {
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("DDS link connects to PX4 flight controllers via CycloneDDS. "
                                     + "Select a profile matching your PX4 firmware version. "
                                     + "Domain ID must match the PX4 DDS domain (default 0).\n\n"
                                     + "PX4 SITL v1.17 — Software-in-the-loop simulation (latest)\n"
                                     + "PX4 SITL v1.16 — Software-in-the-loop simulation (v1.16)\n"
                                     + "CUAV X7+ (v1.16) — CUAV X7+ Pro hardware with v1.16 firmware")
    }
}
