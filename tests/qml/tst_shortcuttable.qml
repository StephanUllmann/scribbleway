import QtQuick
import QtQml
import QtTest

// main.qml drives its ~30 tool/action/nudge hotkeys from two Instantiator tables rather
// than one Shortcut block each. A Shortcut only fires if the context matcher can walk
// its parent chain up to the focused window, and an Instantiator adds a hop to that
// chain — so pin that a table-built Shortcut actually reaches the key, not just that it
// was constructed. Losing every hotkey silently is this app's worst failure mode.
TestCase {
    id: testCase
    name: "ShortcutTable"
    when: windowShown
    width: 100
    height: 100
    visible: true

    property var fired: []
    property bool guard: true

    Instantiator {
        model: [
            { seq: "A", run: () => testCase.fired.push("a") },
            { seq: "B", run: () => testCase.fired.push("b") }
        ]
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.seq
            enabled: testCase.guard
            onActivated: modelData.run()
        }
    }

    function init() {
        fired = []
        guard = true
    }

    function test_table_built_shortcut_fires() {
        keyClick(Qt.Key_A)
        compare(fired, ["a"])
        keyClick(Qt.Key_B)
        compare(fired, ["a", "b"])
    }

    function test_guard_disables_the_whole_table() {
        guard = false
        keyClick(Qt.Key_A)
        compare(fired, [])
    }
}
