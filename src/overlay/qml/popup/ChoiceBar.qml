import QtQuick
import QtQuick.Layouts
import ".."

// A segmented control over string options; `current` is the picked index.
Rectangle {
    id: choiceBar
    property var options: []
    property int current: -1
    signal picked(int index)

    Theme { id: theme }

    Layout.fillWidth: true
    implicitHeight: 30
    radius: theme.radius
    color: theme.surface
    RowLayout {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 2
        Repeater {
            model: choiceBar.options
            SegmentButton {
                required property int index
                required property string modelData
                text: modelData
                checked: index === choiceBar.current
                onClicked: choiceBar.picked(index)
            }
        }
    }
}
