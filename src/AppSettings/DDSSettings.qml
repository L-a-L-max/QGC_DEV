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

        QGCLabel { text: qsTr("Vendor Mapping") }
        QGCTextField {
            id:                     vendorField
            text:                   subEditConfig.vendorMapping
            Layout.preferredWidth:  _secondColumnWidth
            placeholderText:        qsTr("Leave empty for default PX4")
            onTextChanged:          subEditConfig.vendorMapping = vendorField.text
        }
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
                                     + "The vendor mapping selects which JSON topic-mapping table to load. "
                                     + "Domain ID must match the PX4 DDS domain (default 0).")
    }
}
