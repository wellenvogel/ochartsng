#imported from https://libspng.org/
cmake_minimum_required(VERSION 3.0)

project(libspng C)

set(SPNG_MAJOR 0)
set(SPNG_MINOR 7)
set(SPNG_REVISION 2)
set(SPNG_VERSION ${SPNG_MAJOR}.${SPNG_MINOR}.${SPNG_REVISION})

option(ENABLE_OPT "Enable architecture-specific optimizations" ON)
option(SPNG_SHARED "Build shared lib" OFF)
option(SPNG_STATIC "Build static lib" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

include(GNUInstallDirs)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

set(spng_SOURCES spng/spng.c)
if(NOT CMAKE_HOST_WIN32)
    set(spng_LIBS -lm ${ZLIB_LIBRARIES})
else()
    set(spng_LIBS ${ZLIB_LIBRARIES})
endif()

if(NOT ENABLE_OPT)
    add_definitions( -DSPNG_DISABLE_OPT=1 )
endif()

if(SPNG_SHARED)
    add_library(spng SHARED ${spng_SOURCES})
    target_link_libraries(spng ${spng_LIBS})
    install(TARGETS spng DESTINATION lib)

    if(BUILD_EXAMPLES)
        add_executable(example examples/example.c)
        target_include_directories(example PRIVATE ${PROJECT_SOURCE_DIR}/spng)
        target_link_libraries(example spng ${spng_LIBS})
    endif()
endif()

if(SPNG_STATIC)
    add_library(spng STATIC ${spng_SOURCES})
    target_compile_definitions(spng PUBLIC SPNG_STATIC)
    target_link_libraries(spng ${spng_LIBS})
    install(TARGETS spng DESTINATION lib)
endif()
target_include_directories(spng INTERFACE spng)
install(FILES spng/spng.h DESTINATION include)


