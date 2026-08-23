cmake_minimum_required(VERSION 3.24)

project(atoll
    VERSION 0.1.0
    DESCRIPTION "A dynamic island for KDE Plasma"
    HOMEPAGE_URL "https://github.com/atoll-shell/atoll"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(FeatureSummary)

find_package(ECM 6.0 QUIET)
if(ECM_FOUND)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 DBus Svg Network ShaderTools)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n)
find_package(LayerShellQt REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBus1 REQUIRED IMPORTED_TARGET dbus-1)

qt_standard_project_setup(REQUIRES 6.6)

# Atoll keeps its QML in views/ and components/ subdirectories but exposes them
# as one flat "Atoll" module, which is the pre-QTP0004 behaviour.
if(POLICY CMP0000)
    qt_policy(SET QTP0004 OLD)
endif()
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

qt_add_executable(atoll
    src/main.cpp

    src/app/application.cpp        src/app/application.h
    src/app/shellwindow.cpp        src/app/shellwindow.h
    src/app/imagestore.cpp         src/app/imagestore.h

    src/config/config.cpp          src/config/config.h

    src/dbus/dbusmonitor.cpp       src/dbus/dbusmonitor.h
    src/dbus/dbusvariant.cpp       src/dbus/dbusvariant.h
    src/dbus/osdmonitor.cpp        src/dbus/osdmonitor.h
    src/dbus/notificationmonitor.cpp src/dbus/notificationmonitor.h
    src/dbus/notificationmodel.cpp src/dbus/notificationmodel.h
    src/dbus/mprisplayer.cpp       src/dbus/mprisplayer.h
    src/dbus/mprismanager.cpp      src/dbus/mprismanager.h

    src/system/battery.cpp         src/system/battery.h
    src/system/clock.cpp           src/system/clock.h
    src/system/visualizer.cpp      src/system/visualizer.h

    src/ipc/ipcservice.cpp         src/ipc/ipcservice.h
)

# The generated QML type registration includes headers by base name, so every
# directory holding a QML-exposed type has to be reachable directly.
target_include_directories(atoll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dbus
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system
)

target_compile_definitions(atoll PRIVATE
    ATOLL_VERSION="${PROJECT_VERSION}"
)

# Theme and Cfg are singletons; Qt needs that recorded on the files themselves.
set_source_files_properties(qml/Theme.qml qml/Cfg.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(atoll
    URI Atoll
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Island.qml
        qml/Theme.qml
        qml/Cfg.qml
        qml/Stage.qml
        qml/views/IdleView.qml
        qml/views/OsdView.qml
        qml/views/NotificationView.qml
        qml/views/MediaView.qml
        qml/views/ExpandedView.qml
        qml/views/CallToActionView.qml
        qml/components/Blob.qml
        qml/components/GooLayer.qml
        qml/components/IconImage.qml
        qml/components/Marquee.qml
        qml/components/LevelBar.qml
        qml/components/ProgressRing.qml
        qml/components/Spectrum.qml
        qml/components/AlbumArt.qml
        qml/components/RoundButton.qml
        qml/components/QuickToggle.qml
        qml/components/Ripple.qml
    RESOURCES
        data/icons/atoll.svg
)

qt6_add_shaders(atoll "atoll_shaders"
    BATCHABLE
    PREFIX "/qt/qml/Atoll/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    FILES
        shaders/gooey.frag
)

target_link_libraries(atoll PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::DBus
    Qt6::Network
    Qt6::Svg
    KF6::CoreAddons
    KF6::I18n
    LayerShellQtInterface
    PkgConfig::DBus1
)

install(TARGETS atoll RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/io.github.atoll.Atoll.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES data/icons/atoll.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
        RENAME io.github.atoll.Atoll.svg)
install(FILES packaging/atoll.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user)
install(PROGRAMS packaging/atollctl
        DESTINATION ${CMAKE_INSTALL_BINDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
