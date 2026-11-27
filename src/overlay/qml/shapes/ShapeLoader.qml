import QtQuick

// Loads the delegate that draws a Shape of `type`. The canvas and the Tray Popup's
// specimen well both use it, so a new Shape type is wired up in one place.
Loader {
    property string type

    function fileFor(type) {
        switch (type.toLowerCase()) {
            case "rectangle": return "RectangleShape.qml"
            case "ellipse": return "EllipseShape.qml"
            case "line": return "LineShape.qml"
            case "arrow": return "ArrowShape.qml"
            case "freehand": return "FreehandShape.qml"
            case "text": return "TextShape.qml"
        }
        return ""
    }

    anchors.fill: parent
    // A bare relative source would resolve against the caller's file; resolvedUrl
    // resolves against this one, so callers in any directory get the delegate.
    source: fileFor(type) !== "" ? Qt.resolvedUrl(fileFor(type)) : ""
}
