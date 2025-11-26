

add_compile_options(-pthread -fPIC)

set(COLLATZ_SOURCES collatz.cpp)
set(COLLATZ_HEADERS collatz.h)

add_library(collatzlib STATIC
    ${COLLATZ_SOURCES}
    ${COLLATZ_HEADERS}
)

target_compile_options(collatzlib PRIVATE
    -O3
    -march=native
    -mtune=native
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(collatzlib PRIVATE
        -ffast-math
        -flto
        -funroll-loops
        -falign-functions=32
        -fomit-frame-pointer
    )
endif()

find_package(Threads QUIET)
if(Threads_FOUND)
    target_link_libraries(collatzlib PRIVATE Threads::Threads)
endif()

set_target_properties(collatzlib PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    PUBLIC_HEADER collatz.h
)

target_include_directories(collatzlib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

