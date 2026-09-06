import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: sidebar

    property bool darkMode: false
    property string currentPagePath: ""
    property int sidebarWidth: 240
    property alias currentIndex: treeView.currentIndex

    width: sidebarWidth
    color: backend.themeBackground

    focus: true

    Keys.onUpPressed: function(event) {
        if (notebookManager.count === 0) return;
        if (treeView.currentIndex <= 0)
            treeView.currentIndex = notebookManager.count - 1;
        else
            treeView.currentIndex--;
        event.accepted = true;
    }

    Keys.onDownPressed: function(event) {
        if (notebookManager.count === 0) return;
        if (treeView.currentIndex >= notebookManager.count - 1)
            treeView.currentIndex = 0;
        else
            treeView.currentIndex++;
        event.accepted = true;
    }

    Keys.onReturnPressed: function(event) {
        if (treeView.currentIndex < 0) return;
        var item = notebookManager.itemAt(treeView.currentIndex);
        if (item.isPage) {
            sidebar.pageSelected(item.path);
        } else {
            notebookManager.toggleExpanded(item.path);
        }
        event.accepted = true;
    }

    Keys.onEnterPressed: Keys.onReturnPressed

    Keys.onDeletePressed: function(event) {
        if (treeView.currentIndex < 0) return;
        var item = notebookManager.itemAt(treeView.currentIndex);
        deleteDialog.deleteTarget = item.path;
        deleteDialog.deleteName = item.name;
        deleteDialog.open();
        event.accepted = true;
    }

    Keys.onEscapePressed: function(event) {
        sidebar.closed();
        event.accepted = true;
    }

    Keys.onPressed: function(event) {
        if (treeView.currentIndex < 0) return;
        var item = notebookManager.itemAt(treeView.currentIndex);

        if (event.key === Qt.Key_Up && (event.modifiers & Qt.ControlModifier) && !(event.modifiers & Qt.ShiftModifier) && item.isPage) {
            treeView.movePageInTab(item, -1);
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Down && (event.modifiers & Qt.ControlModifier) && !(event.modifiers & Qt.ShiftModifier) && item.isPage) {
            treeView.movePageInTab(item, 1);
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Up && (event.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) && item.isPage) {
            treeView.movePageToAdjacentTab(item, -1);
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Down && (event.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) && item.isPage) {
            treeView.movePageToAdjacentTab(item, 1);
            event.accepted = true;
        }
        else if (event.key === Qt.Key_F2) {
            renameDialog.renameTarget = item.path;
            renameDialog.currentName = item.name;
            renameDialog.open();
            event.accepted = true;
        }
        else if (event.key === Qt.Key_N && !(event.modifiers & Qt.ControlModifier) && !item.isPage) {
            notebookManager.createPage(item.path, "Untitled");
            event.accepted = true;
        }
    }

    Timer {
        id: focusTimer
        interval: 50
        repeat: false
        onTriggered: sidebar.forceActiveFocus()
    }

    Component.onCompleted: focusTimer.start()

    signal pageSelected(string path)
    signal closed()
    signal openNotebookPicker()
    signal openConvertToNotebook()

    function openNewTabDialog() {
        newTabDialog.open();
    }

    // Top toolbar
    Rectangle {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 40
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 4
            spacing: 4

            Text {
                text: {
                    var nb = notebookManager.currentNotebook;
                    if (nb === "") return "Notebook";
                    return nb.split("/").pop();
                }
                color: sidebar.darkMode ? "#909191" : "#aeb1b5"
                font.family: "iA Writer Mono S"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            // New Tab button
            Button {
                id: newTabBtn
                flat: true
                text: "+"
                font.pixelSize: 16
                font.weight: Font.Bold
                implicitWidth: 28
                implicitHeight: 28
                onClicked: sidebar.openNewTabDialog()

                background: Rectangle {
                    radius: 4
                    color: newTabBtn.hovered
                        ? Qt.lighter(backend.themeBackground, 1.1)
                        : "transparent"
                }

                contentItem: Label {
                    text: newTabBtn.text
                    color: backend.darkMode ? "#909191" : "#aeb1b5"
                    font: newTabBtn.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // Close sidebar button
            Button {
                id: closeBtn
                flat: true
                text: "\u2715"
                font.pixelSize: 12
                implicitWidth: 28
                implicitHeight: 28
                onClicked: sidebar.closed()

                background: Rectangle {
                    radius: 4
                    color: closeBtn.hovered
                        ? Qt.lighter(backend.themeBackground, 1.1)
                        : "transparent"
                }

                contentItem: Label {
                    text: closeBtn.text
                    color: backend.darkMode ? "#909191" : "#aeb1b5"
                    font: closeBtn.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Bottom separator
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Qt.darker(backend.themeBackground, 1.15)
        }
    }

    // Tree view
    ListView {
        id: treeView
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: notebookManager
        spacing: 0
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: -1
        keyNavigationEnabled: false

        function movePageInTab(item, direction) {
            // Find the tab this page belongs to
            var tabPath = item.parentPath;
            if (tabPath === "") return;

            // Get all items and find pages in this tab
            var tabPages = [];
            var targetIndex = -1;
            for (var i = 0; i < notebookManager.count; i++) {
                var cur = notebookManager.itemAt(i);
                if (cur.parentPath === tabPath && cur.isPage) {
                    if (cur.path === item.path) targetIndex = tabPages.length;
                    tabPages.push(cur);
                }
            }

            var newIndex = targetIndex + direction;
            if (newIndex < 0 || newIndex >= tabPages.length) return;

            // Swap files on disk by renaming to temp names
            var a = tabPages[targetIndex];
            var b = tabPages[newIndex];
            var tempPath = tabPath + "/.swap_tmp";

            // Use QFile::rename via the model - we need a reorder method
            // For now, swap the actual files
            QFile.rename(a.path, tempPath);
            QFile.rename(b.path, a.path);
            QFile.rename(tempPath, b.path);
            notebookManager.refresh();
        }

        function movePageToAdjacentTab(item, direction) {
            var tabPath = item.parentPath;
            if (tabPath === "") return;

            // Find adjacent tab
            var tabs = [];
            for (var i = 0; i < notebookManager.count; i++) {
                var cur = notebookManager.itemAt(i);
                if (!cur.isPage && cur.depth === 0) {
                    tabs.push(cur);
                }
            }

            var tabIdx = -1;
            for (var j = 0; j < tabs.length; j++) {
                if (tabs[j].path === tabPath) {
                    tabIdx = j;
                    break;
                }
            }

            if (tabIdx < 0) return;
            var newTabIdx = tabIdx + direction;
            if (newTabIdx < 0 || newTabIdx >= tabs.length) return;

            notebookManager.movePageToTab(item.path, tabs[newTabIdx].path);
        }

        delegate: SidebarItem {
            required property string itemName
            required property string itemPath
            required property int depth
            required property bool isExpanded
            required property bool isPage
            required property int index

            darkMode: sidebar.darkMode
            width: treeView.width
            isActive: itemPath === sidebar.currentPagePath
            isKeyboardSelected: index === treeView.currentIndex && sidebar.activeFocus

            onClicked: {
                treeView.currentIndex = index;
                sidebar.forceActiveFocus();
                if (isPage) {
                    sidebar.pageSelected(itemPath);
                } else {
                    notebookManager.toggleExpanded(itemPath);
                }
            }

            onExpandToggled: {
                notebookManager.toggleExpanded(itemPath);
            }

            onRenameRequested: {
                renameDialog.renameTarget = itemPath;
                renameDialog.currentName = itemName;
                renameDialog.open();
            }

            onDeleteRequested: {
                deleteDialog.deleteTarget = itemPath;
                deleteDialog.deleteName = itemName;
                deleteDialog.open();
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }
    }

    // Empty state - no notebook loaded
    Column {
        anchors.centerIn: treeView
        spacing: 16
        visible: notebookManager.currentNotebook === ""

        Text {
            text: "No notebook loaded"
            color: sidebar.darkMode ? "#909191" : "#aeb1b5"
            font.family: "iA Writer Mono S"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Column {
            spacing: 4
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: "Open Notebook"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: sidebar.openNotebookPicker()
            }

            Text {
                text: "Ctrl+Shift+O"
                color: sidebar.darkMode ? "#666" : "#999"
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        Column {
            spacing: 4
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: "Make into Notebook"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: sidebar.openConvertToNotebook()
            }

            Text {
                text: "Ctrl+Shift+M"
                color: sidebar.darkMode ? "#666" : "#999"
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // Empty state - notebook loaded but no tabs
    Text {
        anchors.centerIn: treeView
        text: "No tabs yet.\nClick + to create one."
        color: sidebar.darkMode ? "#909191" : "#aeb1b5"
        font.family: "iA Writer Mono S"
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
        visible: notebookManager.currentNotebook !== "" && notebookManager.count === 0
    }

    // Dialogs
    Dialog {
        id: newTabDialog
        modal: true
        title: "New Tab"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 300

        contentItem: TextField {
            id: newTabField
            placeholderText: "Tab name"
            selectByMouse: true
            onAccepted: newTabDialog.accept()
            Material.accent: backend.themeAccent
        }

        onOpened: {
            newTabField.text = "";
            newTabField.forceActiveFocus();
        }

        onAccepted: {
            var name = newTabField.text.trim();
            if (name.length > 0) {
                notebookManager.createTab(name);
            }
        }
    }

    Dialog {
        id: renameDialog
        modal: true
        title: "Rename"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 300

        property string renameTarget: ""
        property string currentName: ""

        contentItem: TextField {
            id: renameField
            selectByMouse: true
            onAccepted: renameDialog.accept()
            Material.accent: backend.themeAccent
        }

        onOpened: {
            renameField.text = renameDialog.currentName;
            renameField.selectAll();
            renameField.forceActiveFocus();
        }

        onAccepted: {
            var newName = renameField.text.trim();
            if (newName.length > 0) {
                var currentFile = backend.fileUrl.toString().replace("file://", "");
                if (renameDialog.renameTarget === currentFile) {
                    backend.suppressNextExternalChange();
                }
                notebookManager.renameItem(renameDialog.renameTarget, newName);
            }
        }
    }

    Dialog {
        id: deleteDialog
        modal: true
        title: "Delete"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 300

        property string deleteTarget: ""
        property string deleteName: ""

        contentItem: Label {
            text: "Delete \"" + deleteDialog.deleteName + "\"?\nThis cannot be undone."
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            var currentFile = backend.fileUrl.toString().replace("file://", "");
            if (deleteDialog.deleteTarget === currentFile) {
                backend.suppressNextExternalChange();
            }
            notebookManager.deleteItem(deleteDialog.deleteTarget);
        }
    }

    // Separator line on right edge
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Qt.darker(backend.themeBackground, 1.15)
    }
}
