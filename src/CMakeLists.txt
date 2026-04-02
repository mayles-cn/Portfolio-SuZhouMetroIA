set(APP_TARGET SuZhouMetroIA)

add_subdirectory(models)
add_subdirectory(services)
add_subdirectory(utils)
add_subdirectory(widgets)

set(APP_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    ${CMAKE_SOURCE_DIR}/resources/metro_data.qrc
    ${MODEL_SOURCES}
    ${SERVICE_SOURCES}
    ${UTIL_SOURCES}
    ${WIDGET_SOURCES}
)

qt_add_executable(${APP_TARGET}
    ${APP_SOURCES}
)

target_include_directories(${APP_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${APP_TARGET}
    PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Network
)

if(TARGET Qt6::WebSockets)
    target_link_libraries(${APP_TARGET} PRIVATE Qt6::WebSockets)
    target_compile_definitions(${APP_TARGET} PRIVATE SZMETRO_ENABLE_WEBSOCKET_ASR=1)
else()
    message(WARNING "Qt6WebSockets is unavailable. Realtime ASR will run in disabled fallback mode.")
endif()

if(WIN32)
    set_target_properties(${APP_TARGET} PROPERTIES WIN32_EXECUTABLE ON)
endif()
