qt_add_dbus_interface(wacom_applet_dbus_sources "${CMAKE_CURRENT_SOURCE_DIR}/../kded/org.kde.Wacom.xml" wacomappletinterface)

plasma_add_applet(org.kde.plasma.wacomtablet
    QML_SOURCES
        main.qml
        FullRepresentation.qml
        RotationButton.qml
    CPP_SOURCES
        ${wacom_applet_dbus_sources}
        tabletmodel.h
        tabletmodel.cpp
        multidbuspendingcallwatcher.h
        multidbuspendingcallwatcher.cpp
    GENERATE_APPLET_CLASS
)

target_link_libraries(org.kde.plasma.wacomtablet PRIVATE
    Qt::Core
    Qt::DBus
)
