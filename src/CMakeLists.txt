set(APP_TARGET SuZhouMetroIA)

add_subdirectory(models)
add_subdirectory(services)
add_subdirectory(utils)
add_subdirectory(widgets)

set(APP_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
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
        Qt${QT_VERSION_MAJOR}::Widgets
)

if(WIN32)
    set_target_properties(${APP_TARGET} PROPERTIES WIN32_EXECUTABLE ON)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${APP_TARGET})
endif()
