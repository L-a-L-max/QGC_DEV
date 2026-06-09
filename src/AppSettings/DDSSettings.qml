import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    spacing: _rowSpacing

    function saveSettings() {
        subEditConfig.stopDiscovery()
    }

    // ── Domain ID ──
    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Domain ID") }
        QGCTextField {
            id:                     domainField
            text:                   subEditConfig.domainId.toString()
            focus:                  true
            Layout.preferredWidth:  _secondColumnWidth * 0.5
            inputMethodHints:       Qt.ImhFormattedNumbersOnly
            onTextChanged:          subEditConfig.domainId = parseInt(domainField.text) || 0
        }
        QGCButton {
            text:       subEditConfig.discovering ? qsTr("Stop Scan") : qsTr("Scan")
            onClicked: {
                if (subEditConfig.discovering) {
                    subEditConfig.stopDiscovery()
                } else {
                    subEditConfig.startDiscovery()
                }
            }
        }
    }

    // ── Namespace selection ──
    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Namespace") }

        QGCComboBox {
            id:                     nsCombo
            Layout.preferredWidth:  _secondColumnWidth
            editable:               true
            model:                  subEditConfig.discoveredNamespaces.length > 0
                                        ? subEditConfig.discoveredNamespaces
                                        : [""]
            currentIndex: {
                if (subEditConfig.namespacePrefix.length === 0) return 0
                var idx = subEditConfig.discoveredNamespaces.indexOf(subEditConfig.namespacePrefix)
                return idx >= 0 ? idx : 0
            }

            onActivated: function(index) {
                var ns = model[index] || ""
                subEditConfig.namespacePrefix = ns
            }

            // Handle manually typed text
            onAccepted: {
                subEditConfig.namespacePrefix = editText
            }
        }
    }

    // Manual entry hint
    QGCLabel {
        visible:                subEditConfig.discoveredNamespaces.length === 0 && !subEditConfig.discovering
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("Click 'Scan' to discover namespaces, or type a namespace manually above.")
    }

    // Discovery status
    QGCLabel {
        visible:                subEditConfig.discovering
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   subEditConfig.discoveredNamespaces.length > 0
                                    ? qsTr("Found %1 namespace(s). Scanning...").arg(subEditConfig.discoveredNamespaces.length)
                                    : qsTr("Scanning for DDS namespaces on domain %1...").arg(subEditConfig.domainId)
    }

    // ── Vendor Mapping ──
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

    // ── Help text ──
    QGCLabel {
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("DDS link connects to PX4 flight controllers via CycloneDDS. "
                                     + "Enter the Domain ID and click 'Scan' to discover drones on the network. "
                                     + "Select a namespace to connect to a specific drone. "
                                     + "Leave namespace empty for single-drone (default) mode.")
    }
}
