cmake_minimum_required(VERSION 3.20)

qt_add_executable(scribbleway-overlay
    main.cpp
    overlaycontroller.h
    overlaycontroller.cpp
    shapesmodel.h
    shapesmodel.cpp
)

qt_add_qml_module(scribbleway-overlay
    URI scribbleway
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        qml/main.qml
        qml/shapes/BaseShape.qml
        qml/shapes/RectangleShape.qml
        qml/shapes/EllipseShape.qml
        qml/shapes/LineShape.qml
        qml/shapes/ArrowShape.qml
        qml/shapes/FreehandShape.qml
        qml/shapes/TextShape.qml
)

target_link_libraries(scribbleway-overlay PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    KF6::GlobalAccel
    KF6::DBusAddons
    LayerShellQt::Interface
)

# Install executable to standard bin directory
install(TARGETS scribbleway-overlay DESTINATION ${KDE_INSTALL_BINDIR})
