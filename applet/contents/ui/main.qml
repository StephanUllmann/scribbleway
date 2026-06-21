import QtQuick
import org.kde.plasma.plasmoid
import org.kde.scribbleway.backend

PlasmoidItem {
    id: root

    AppletBackend {
        id: backend
    }

    // Expose backend so sub-items can access it
    property alias backend: backend

    Component.onCompleted: {
        if (Plasmoid.configuration.targetScreen) {
            backend.setTargetScreen(Plasmoid.configuration.targetScreen);
        } else if (backend.screenNames.length > 0) {
            Plasmoid.configuration.targetScreen = backend.screenNames[0];
            backend.setTargetScreen(Plasmoid.configuration.targetScreen);
        }
    }

    Connections {
        target: Plasmoid.configuration
        function onTargetScreenChanged() {
            backend.setTargetScreen(Plasmoid.configuration.targetScreen);
        }
    }

    // Compact representation (icon in the panel)
    compactRepresentation: CompactRepresentation {}

    // Full representation (the settings/controls popup)
    fullRepresentation: FullRepresentation {}

    preferredRepresentation: compactRepresentation
}
