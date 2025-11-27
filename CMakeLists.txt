
cmake_minimum_required(VERSION 3.16)
project(AlgorithmApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(USE_QT6 "Prefer Qt6 if available" ON)

if(USE_QT6)
    find_package(Qt6 COMPONENTS Core Widgets Concurrent QUIET)
endif()

if(NOT TARGET Qt6::Core)
    find_package(Qt5 COMPONENTS Core Widgets Concurrent REQUIRED)
    set(Qt_LIBS Qt5::Widgets Qt5::Concurrent Qt5::Core)
else()
    set(Qt_LIBS Qt6::Widgets Qt6::Concurrent Qt6::Core)
endif()


if(MSVC)
    # Windows/MSVC
    add_compile_options(/W4 /EHsc /utf-8)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
else()
    # Unix/Linux/macOS
    add_compile_options(-Wall -Wextra -pthread -fPIC)
endif()


find_package(Qt6 COMPONENTS Core Widgets Concurrent QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Widgets Concurrent REQUIRED)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# Add subdirectories
add_subdirectory(lib)
add_subdirectory(app)

