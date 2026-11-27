import QtQuick
import QtQuick.Layouts
import ".."

// One aligned row: fixed label column, the controls, and an optional value column.
RowLayout {
    id: fieldRow
    property alias label: fieldLabel.text
    property string value: ""
    default property alias content: slot.data

    Theme { id: theme }

    Layout.fillWidth: true
    Layout.minimumHeight: 34
    spacing: 10
    Label {
        id: fieldLabel
        Layout.preferredWidth: theme.labelWidth
        color: theme.mute
        elide: Text.ElideRight
    }
    RowLayout {
        id: slot
        Layout.fillWidth: true
        spacing: 7
    }
    Label {
        visible: fieldRow.value !== ""
        text: fieldRow.value
        Layout.preferredWidth: theme.valueWidth
        horizontalAlignment: Text.AlignRight
        font.features: { "tnum": 1 }    // values stay put while a slider drags
    }
}
