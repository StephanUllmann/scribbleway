cmake_minimum_required(VERSION 3.20)
project(scribbleway VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KF_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-missing-include-dirs)
endif()

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
    DBus
)

include(ECMQmlModule)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    GlobalAccel
)

find_package(LayerShellQt REQUIRED)

add_subdirectory(src/overlay)
enable_testing()
add_subdirectory(tests)

install(FILES org.kde.scribbleway.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.scribbleway-autostart.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})

option(BUILD_DEB_PACKAGE "Automatically rebuild the .deb package after a successful build" ON)

if(BUILD_DEB_PACKAGE)
    if(NOT DEFINED ENV{DEB_BUILD_ARCH} AND NOT DEFINED ENV{DPKG_RUNNING_VERSION})
        add_custom_target(rebuild_debian_package ALL
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/packaging/build_deb.sh ${CMAKE_BINARY_DIR}
            DEPENDS scribbleway-overlay
            COMMENT "Building Debian package (.deb) from build outputs"
            VERBATIM
        )
    endif()
endif()
