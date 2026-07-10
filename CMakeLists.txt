cmake_minimum_required(VERSION 3.20)
project(scribbleway VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KF_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")

# Find Extra CMake Modules (ECM) from KDE Frameworks
find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# Find Qt6 components
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    DBus
)

include(ECMQmlModule)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    GlobalAccel
    DBusAddons
)

option(BUILD_PLASMA_APPLET "Build Plasma applet package and QML backend plugin" ON)

# Find Plasma 6
if(BUILD_PLASMA_APPLET)
    find_package(Plasma REQUIRED)
endif()

# Find LayerShellQt
find_package(LayerShellQt REQUIRED)

# Add subdirectories
if(BUILD_PLASMA_APPLET)
    add_subdirectory(src/applet-plugin)
endif()
add_subdirectory(src/overlay)
enable_testing()
add_subdirectory(tests)

# Install the applet package
if(BUILD_PLASMA_APPLET)
    plasma_install_package(applet org.kde.scribbleway)
endif()

# Install desktop file for shortcuts configuration and autostart
install(FILES org.kde.scribbleway.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.scribbleway-autostart.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})

# Automatically rebuild the .deb package after a successful build
option(BUILD_DEB_PACKAGE "Automatically rebuild the .deb package after a successful build" ON)

if(BUILD_DEB_PACKAGE)
    if(NOT DEFINED ENV{DEB_BUILD_ARCH} AND NOT DEFINED ENV{DPKG_RUNNING_VERSION})
        set(_deb_depends scribbleway-overlay)
        if(BUILD_PLASMA_APPLET)
            list(APPEND _deb_depends scribblewaybackend)
        endif()
        add_custom_target(rebuild_debian_package ALL
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/packaging/build_deb.sh ${CMAKE_BINARY_DIR}
            DEPENDS ${_deb_depends}
            COMMENT "Building Debian package (.deb) from build outputs"
            VERBATIM
        )
    endif()
endif()

