import QtQuick
import QtQuick.Window

Window {
    id: trayPopup
    width: 432
    height: 550
    minimumWidth: 400
    minimumHeight: 520
    flags: Qt.Popup | Qt.FramelessWindowHint
    visible: false
    color: sysPalette.window

    SystemPalette {
        id: sysPalette
    }

    FullRepresentation {
        anchors.fill: parent
        anchors.margins: 8
    }
}
