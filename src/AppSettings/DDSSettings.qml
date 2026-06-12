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
            model:                  ["PX4 SITL (default)", "CUAV X7+ Pro", "Custom..."]
            property var profileValues: ["", "cuav_x7pro", "_custom"]
            currentIndex: {
                var idx = profileValues.indexOf(subEditConfig.vendorMapping)
                if (idx >= 0) return idx
                if (subEditConfig.vendorMapping.length > 0) return 2  // custom
                return 0
            }
            onActivated: function(idx) {
                if (profileValues[idx] === "_custom") {
                    customField.visible = true
                } else {
                    customField.visible = false
                    subEditConfig.vendorMapping = profileValues[idx]
                }
            }
        }
    }

    QGCTextField {
        id:                     customField
        Layout.fillWidth:       true
        visible:                profileCombo.currentIndex === 2
        text:                   (subEditConfig.vendorMapping !== "" && subEditConfig.vendorMapping !== "cuav_x7pro")
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
                                     + "Select a built-in profile or 'Custom' to use your own JSON mapping. "
                                     + "Domain ID must match the PX4 DDS domain (default 0).")
    }
}
