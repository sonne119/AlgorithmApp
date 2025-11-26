cmake_minimum_required(VERSION 3.16...4.2)

project(AlgorithmApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt (try Qt6 first, fallback to Qt5)
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

# Add subdirectories
add_subdirectory(lib)
add_subdirectory(app)
