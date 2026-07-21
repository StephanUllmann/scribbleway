find_package(Qt6 REQUIRED COMPONENTS Test Quick DBus)
find_package(KF6 REQUIRED COMPONENTS GlobalAccel)
find_package(LayerShellQt REQUIRED)

add_executable(shapesmodeltest
    shapesmodeltest.cpp
    ../src/overlay/shapesmodel.h
    ../src/overlay/shapesmodel.cpp
    ../src/overlay/overlaycontroller.h
    ../src/overlay/overlaycontroller.cpp
    ../src/applet-plugin/appletbackend.h
    ../src/applet-plugin/appletbackend.cpp
)

target_include_directories(shapesmodeltest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/overlay
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/applet-plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common
)

target_link_libraries(shapesmodeltest PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Quick
    Qt6::DBus
    KF6::GlobalAccel
    LayerShellQt::Interface
)

add_test(NAME shapesmodeltest COMMAND shapesmodeltest)
