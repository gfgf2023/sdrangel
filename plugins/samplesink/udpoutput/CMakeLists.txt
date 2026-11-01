project(udpoutput)

set(udpoutput_SOURCES
    udpoutput.cpp
    udpoutputplugin.cpp
    udpoutputsettings.cpp
    udpoutputworker.cpp
)

set(udpoutput_HEADERS
    udpoutput.h
    udpoutputplugin.h
    udpoutputsettings.h
    udpoutputworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(udpoutput_SOURCES
        ${udpoutput_SOURCES}
        udpoutputgui.cpp
        udpoutputgui.ui
    )
    set(udpoutput_HEADERS
        ${udpoutput_HEADERS}
        udpoutputgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}outputudpoutput)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}outputudpoutputsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${udpoutput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME UDPOutputPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${udpoutput_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::Network
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
