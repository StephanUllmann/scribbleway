cmake_minimum_required(VERSION 3.20)

ecm_add_qml_module(scribblewaybackend
    URI "org.kde.scribbleway.backend"
    GENERATE_PLUGIN_SOURCE
)

target_sources(scribblewaybackend PRIVATE
    appletbackend.h
    appletbackend.cpp
)

target_link_libraries(scribblewaybackend PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
    Qt6::Gui
)

ecm_finalize_qml_module(scribblewaybackend
    DESTINATION ${KDE_INSTALL_QMLDIR}
)
