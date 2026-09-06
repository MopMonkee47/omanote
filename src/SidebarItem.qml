import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property bool isActive: false
    property bool isKeyboardSelected: false

    signal clicked()
    signal expandToggled()
    signal renameRequested()
    signal deleteRequested()

    width: parent ? parent.width : 200
    height: 36
    color: root.isKeyboardSelected
        ? Qt.lighter(backend.themeBackground, 1.12)
        : (activeFocus || mouseArea.containsMouse
            ? Qt.lighter(backend.themeBackground, 1.08)
            : (root.isActive ? Qt.darker(backend.themeBackground, 0.92) : "transparent"))

    property bool darkMode: false

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8 + root.depth * 16
        anchors.rightMargin: 8
        spacing: 6

        // Expand/collapse arrow for tabs
        Text {
            id: arrowText
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            visible: !root.isPage
            text: root.isExpanded ? "\u25BC" : "\u25B6"
            color: root.darkMode ? "#909191" : "#aeb1b5"
            font.pixelSize: 9
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
        }

        // Placeholder for pages (no arrow)
        Item {
            Layout.preferredWidth: 14
            visible: root.isPage
        }

        // Icon
        Text {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            text: {
                if (root.isPage) return "\uD83D\uDCC4" // page icon
                return "\uD83D\uDCD1" // folder/tab icon
            }
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            visible: false // hide emoji icons, use text markers instead
        }

        // Name
        Text {
            Layout.fillWidth: true
            text: root.itemName
            color: root.darkMode ? "#c8c8c8" : "#1d1d1f"
            font.pixelSize: 13
            font.weight: root.isActive ? Font.Bold : Font.Normal
            elide: Text.ElideRight
            maximumLineCount: 1
            renderType: Text.NativeRendering
            verticalAlignment: Text.AlignVCenter
        }

        // Page count for tabs (optional)
        Label {
            visible: !root.isPage
            text: ""
            color: root.darkMode ? "#666" : "#999"
            font.pixelSize: 11
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        z: 1
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                root.renameRequested();
            }
        }
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                contextMenu.popup();
            } else if (!root.isPage && mouse.x < 8 + root.depth * 16 + 20) {
                root.expandToggled();
            } else {
                root.clicked();
            }
        }
    }

    Menu {
        id: contextMenu
        Material.theme: root.darkMode ? Material.Dark : Material.Light
        Material.accent: backend.themeAccent

        MenuItem {
            text: "New Page"
            visible: !root.isPage
            onTriggered: {
                var path = root.isPage ? "" : root.itemPath;
                notebookManager.createPage(path, "Untitled");
            }
        }
        MenuItem {
            text: "Rename"
            onTriggered: root.renameRequested()
        }
        MenuSeparator { }
        MenuItem {
            text: "Delete"
            onTriggered: root.deleteRequested()
        }
    }

    function updateHeight() {
        height = 36;
    }
}
