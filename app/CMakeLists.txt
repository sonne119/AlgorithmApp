# App executable CMakeLists (subdirectory)

set(APP_SOURCES
    main.cpp
    mainwindow.cpp
    CollatzRunner.cpp
)

set(APP_HEADERS
    mainwindow.h
    CollatzRunner.h
)

add_executable(app
    ${APP_SOURCES}
    ${APP_HEADERS}
    mainwindow.ui
    CMakeLists.txt
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(app PRIVATE
    collatzlib
    ${Qt_LIBS}
)

set_target_properties(app PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
