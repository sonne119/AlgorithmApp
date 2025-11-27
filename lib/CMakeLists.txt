
if(MSVC)
    # Windows
    add_compile_options(/W4 /EHsc /wd4324)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # Unix/macOS
    add_compile_options(-pthread -fPIC)
endif()

set(COLLATZ_SOURCES
    collatz.cpp
)

set(COLLATZ_HEADERS
    collatz.h
    platform_compat.h
)

add_library(collatzlib STATIC
    ${COLLATZ_SOURCES}
    ${COLLATZ_HEADERS}
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(collatzlib PRIVATE
            /O2
            /Oi
            /GL
            /favor:AMD64
            /fp:fast
        )
        set_target_properties(collatzlib PROPERTIES
            STATIC_LIBRARY_FLAGS
        )
    endif()
else()
    # Unix/macOS
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
endif()

find_package(Threads REQUIRED)
target_link_libraries(collatzlib PRIVATE Threads::Threads)

set_target_properties(collatzlib PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    PUBLIC_HEADER "collatz.h;platform_compat.h"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(collatzlib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
