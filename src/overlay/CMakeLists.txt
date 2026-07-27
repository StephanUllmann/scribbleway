cmake_minimum_required(VERSION 3.20)

qt_add_executable(scribbleway-overlay
    main.cpp
    overlaycontroller.h
    overlaycontroller.cpp
    shapesmodel.h
    shapesmodel.cpp
)

qt_policy(SET QTP0004 NEW)

qt_add_qml_module(scribbleway-overlay
    URI scribbleway
    VERSION 1.0
    RESOURCE_PREFIX "/"
    NO_IMPORT_SCAN
    QML_FILES
        qml/main.qml
        qml/ToolCursorBadge.qml
        qml/shapes/BaseShape.qml
        qml/shapes/RectangleShape.qml
        qml/shapes/EllipseShape.qml
        qml/shapes/LineShape.qml
        qml/shapes/ArrowShape.qml
        qml/shapes/FreehandShape.qml
        qml/shapes/TextShape.qml
        qml/shapes/RoughPathGenerator.js
        qml/shapes/RoughStroke.qml
        qml/FullRepresentation.qml
        qml/TrayPopup.qml
)

qt_add_resources(scribbleway-overlay "icons"
    PREFIX "/icons"
    BASE icons
    FILES
        icons/arrow-down.svg
        icons/arrow-right.svg
        icons/color-picker.svg
        icons/draw-freehand.svg
        icons/draw-rectangle.svg
        icons/draw-ellipse.svg
        icons/draw-line.svg
        icons/draw-arrow.svg
        icons/draw-text.svg
        icons/edit-clear.svg
        icons/edit-delete.svg
        icons/edit-redo.svg
        icons/edit-select.svg
        icons/edit-undo.svg
        icons/go-down.svg
        icons/go-up.svg
        icons/object-locked.svg
        icons/object-unlocked.svg
        icons/run-build.svg
)

target_link_libraries(scribbleway-overlay PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::Widgets
    Qt6::DBus
    KF6::GlobalAccel
    LayerShellQt::Interface
)

# Install executable to standard bin directory
install(TARGETS scribbleway-overlay DESTINATION ${KDE_INSTALL_BINDIR})
