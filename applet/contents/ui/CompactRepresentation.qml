import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

Item {
    id: compactRoot

    implicitWidth: Kirigami.Units.iconSizes.medium
    implicitHeight: Kirigami.Units.iconSizes.medium

    Kirigami.Icon {
        anchors.fill: parent
        source: "draw-freehand"
        opacity: root.backend.overlayConnected ? 1.0 : 0.4
        
        // Add a small red dot if not running/connected
        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 8
            height: 8
            radius: 4
            color: "red"
            visible: !root.backend.overlayConnected
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.expanded = !root.expanded
    }
}
