cmake_minimum_required(VERSION 3.20)
project(scribbleway VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KF_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")

# Find Extra CMake Modules (ECM) from KDE Frameworks
find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)

# Find Qt6 components
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    DBus
)

# Find KDE Frameworks 6 components
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    GlobalAccel
    DBusAddons
)

# Find Plasma 6
find_package(Plasma REQUIRED)

# Find LayerShellQt
find_package(LayerShellQt REQUIRED)

# Add subdirectories
add_subdirectory(src/applet-plugin)
add_subdirectory(src/overlay)
enable_testing()
add_subdirectory(tests)

# Install the applet package
plasma_install_package(applet org.kde.scribbleway)

# Install desktop file for shortcuts configuration and autostart
install(FILES scribbleway.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES scribbleway-autostart.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
