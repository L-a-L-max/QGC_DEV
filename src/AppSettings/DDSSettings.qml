import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    spacing: _rowSpacing

    property bool _hasConfig: subEditConfig !== null && subEditConfig !== undefined

    function saveSettings() {
        if (!_hasConfig) return
        // Sync the currently displayed ComboBox text to the config before save.
        // This handles the case where the user sees a namespace in the dropdown
        // but hasn't explicitly clicked on it (currentIndex binding doesn't
        // trigger onActivated).
        if (nsCombo.currentText.length > 0) {
            subEditConfig.namespacePrefix = nsCombo.currentText
        }
        subEditConfig.stopDiscovery()
    }

    // When discovery finds namespaces and current prefix is empty,
    // auto-select the first discovered namespace.
    Connections {
        target: _hasConfig ? subEditConfig : null
        function onDiscoveredNamespacesChanged() {
            if (!_hasConfig) return
            var nsList = subEditConfig.discoveredNamespaces
            if (nsList.length > 0 && subEditConfig.namespacePrefix.length === 0) {
                // Skip empty-string entries
                for (var i = 0; i < nsList.length; i++) {
                    if (nsList[i].length > 0) {
                        subEditConfig.namespacePrefix = nsList[i]
                        break
                    }
                }
            }
        }
    }

    // ── Domain ID ──
    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Domain ID") }
        QGCTextField {
            id:                     domainField
            text:                   _hasConfig ? subEditConfig.domainId.toString() : "0"
            focus:                  true
            Layout.preferredWidth:  _secondColumnWidth * 0.5
            inputMethodHints:       Qt.ImhFormattedNumbersOnly
            onTextChanged: {
                if (_hasConfig) subEditConfig.domainId = parseInt(domainField.text) || 0
            }
        }
        QGCButton {
            text:       _hasConfig && subEditConfig.discovering ? qsTr("Stop Scan") : qsTr("Scan")
            enabled:    _hasConfig
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
            model: {
                if (!_hasConfig) return [""]
                return subEditConfig.discoveredNamespaces.length > 0
                       ? subEditConfig.discoveredNamespaces
                       : [""]
            }
            currentIndex: {
                if (!_hasConfig) return 0
                if (subEditConfig.namespacePrefix.length === 0) return 0
                var nsList = _hasConfig ? subEditConfig.discoveredNamespaces : []
                var idx = nsList.indexOf(subEditConfig.namespacePrefix)
                return idx >= 0 ? idx : 0
            }

            onActivated: function(index) {
                if (!_hasConfig) return
                var ns = model[index] || ""
                subEditConfig.namespacePrefix = ns
            }

            // Handle manually typed text
            onAccepted: {
                if (_hasConfig) subEditConfig.namespacePrefix = editText
            }
        }
    }

    // Manual entry hint
    QGCLabel {
        visible:                _hasConfig && subEditConfig.discoveredNamespaces.length === 0 && !subEditConfig.discovering
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text:                   qsTr("Click 'Scan' to discover namespaces, or type a namespace manually above.")
    }

    // Discovery status
    QGCLabel {
        visible:                _hasConfig && subEditConfig.discovering
        Layout.preferredWidth:  _secondColumnWidth
        Layout.fillWidth:       true
        font.pointSize:         ScreenTools.smallFontPointSize
        wrapMode:               Text.WordWrap
        text: {
            if (!_hasConfig) return ""
            return subEditConfig.discoveredNamespaces.length > 0
                   ? qsTr("Found %1 namespace(s). Scanning...").arg(subEditConfig.discoveredNamespaces.length)
                   : qsTr("Scanning for DDS namespaces on domain %1...").arg(subEditConfig.domainId)
        }
    }

    // ── Vendor Mapping ──
    RowLayout {
        spacing: _colSpacing

        QGCLabel { text: qsTr("Vendor Mapping") }
        QGCTextField {
            id:                     vendorField
            text:                   _hasConfig ? subEditConfig.vendorMapping : ""
            Layout.preferredWidth:  _secondColumnWidth
            placeholderText:        qsTr("Leave empty for default PX4")
            onTextChanged: {
                if (_hasConfig) subEditConfig.vendorMapping = vendorField.text
            }
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
