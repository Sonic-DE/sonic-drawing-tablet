set(tabletmodeltest_SRCS
    tabletmodeltest.cpp
    ../../../src/applet/tabletmodel.cpp
    ../../../src/applet/multidbuspendingcallwatcher.cpp
)
qt_add_dbus_interface(tabletmodeltest_SRCS ../../../src/kded/org.kde.Wacom.xml wacomappletinterface)

add_executable(tabletmodeltest ${tabletmodeltest_SRCS})
target_include_directories(tabletmodeltest PRIVATE ../../../src/applet ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(tabletmodeltest PRIVATE Qt::Core Qt::DBus Qt::Qml Qt::Test)
add_test(NAME tabletmodeltest COMMAND dbus-run-session $<TARGET_FILE:tabletmodeltest>)

# Manual hardware-backed test. This is intentionally not registered with CTest:
# it requires a running org.kde.Wacom service and a physical tablet.
set(tabletmodellivetest_SRCS
    tabletmodellivetest.cpp
    ../../../src/applet/tabletmodel.cpp
    ../../../src/applet/multidbuspendingcallwatcher.cpp
)
qt_add_dbus_interface(tabletmodellivetest_SRCS ../../../src/kded/org.kde.Wacom.xml wacomappletinterface)
add_executable(tabletmodellivetest EXCLUDE_FROM_ALL ${tabletmodellivetest_SRCS})
target_include_directories(tabletmodellivetest PRIVATE ../../../src/applet ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(tabletmodellivetest PRIVATE Qt::Core Qt::DBus Qt::Qml Qt::Test)
