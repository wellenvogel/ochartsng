cmake_minimum_required(VERSION 3.18)
set(CMAKE_CXX_STANDARD 17)

#command line
option(NO_TEST "skip test compilation" OFF)
if (NOT AVNAV_VERSION)
    set(AVNAV_VERSION "development")
endif()
if (NOT OCHARTS_VERSION)
    set(OCHARTS_VERSION "1.0.29.0")
endif()   

# set the project name
project(AvNavOchartsProvider)
#target name
set(TARGET AvnavOchartsProvider)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi -Werror=return-type")
endif()
set(ANDROID_EXE libAvnavOchartsProvider.so)
     

include(FetchContent)
FetchContent_declare(
    ochartspi
    GIT_REPOSITORY https://github.com/bdbcat/o-charts_pi.git
    GIT_TAG ${OCHARTS_VERSION}
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/o-charts_pi
)
fetchContent_Populate(ochartspi)



set(SRC 
    src/StringHelper.cpp
    src/HTTPd/HTTPServer.cpp
    src/HTTPd/Worker.cpp
    src/Logger.cpp
    src/FileHelper.cpp
    src/PngHandler.cpp
    src/DrawingContext.cpp
    src/Renderer.cpp
    src/OexControl.cpp
    src/StatusCollector.cpp
    src/MD5.cpp
    src/ChartInfo.cpp
    src/Chart.cpp
    src/ChartSetInfo.cpp
    src/ChartSet.cpp
    src/ChartCache.cpp
    src/ChartFactory.cpp
    src/SystemHelper.cpp
    src/ChartManager.cpp
    src/OESUChart.cpp
    src/ChartInstaller.cpp
    src/S57Object.cpp
    src/TestRenderer.cpp
    src/FontManager.cpp
    src/S57.cpp
    src/OcAllocator.cpp
    src/RenderHelper.cpp
    src/SettingsManager.cpp
    src/TokenHandler.cpp
    src/s52/S52Data.cpp
    src/s52/S52Symbols.cpp
    src/s52/S52CondRules.cpp
    src/s52/S52Rules.cpp
    src/s52/S52TextParser.cpp
    src/s52/S52SymbolCache.cpp
    lib/simplejson/json.cpp
)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.h include/generated/*.h)

#generated stuff
set(GENERATED_DIR ${CMAKE_CURRENT_LIST_DIR}/include/generated)
set(S57GENERATOR ${CMAKE_CURRENT_LIST_DIR}/tools/generateS57.py)
set(OBJECT_CLASSES ${GENERATED_DIR}/S57ObjectClasses.h)
set(OBJECT_CLASSES_IN ${CMAKE_CURRENT_LIST_DIR}/s57static/s57objectclasses.csv)
add_custom_command(
    OUTPUT ${OBJECT_CLASSES}
    COMMAND ${S57GENERATOR} classes ${OBJECT_CLASSES_IN} ${OBJECT_CLASSES}
    DEPENDS ${OBJECT_CLASSES_IN} ${S57GENERATOR}
    VERBATIM
)

set(ATTRIDS ${GENERATED_DIR}/S57AttributeIds.h)
set(ATTRIDS_IN ${CMAKE_CURRENT_LIST_DIR}/s57static/s57attributes.csv)
add_custom_command(
    OUTPUT ${ATTRIDS}
    COMMAND ${S57GENERATOR} attributes ${ATTRIDS_IN} ${ATTRIDS}
    DEPENDS ${ATTRIDS_IN} ${S57GENERATOR}
    VERBATIM
)

set(ATTRVALS ${GENERATED_DIR}/S57AttributeValues.h)
set(ATTRVALS_IN ${CMAKE_CURRENT_LIST_DIR}/s57static/s57expectedinput.csv)
add_custom_command(
    OUTPUT ${ATTRVALS}
    COMMAND ${S57GENERATOR} attrvalues ${ATTRVALS_IN} ${ATTRVALS}
    DEPENDS ${ATTRVALS_IN} ${S57GENERATOR}
    VERBATIM
)

set(RSETTINGS ${GENERATED_DIR}/RenderSettings.h)
set(USETTINGS ${GENERATED_DIR}/UserSettings.h)
set(SETTINGSIN ${CMAKE_CURRENT_LIST_DIR}/settings/Settings.csv)
set(SETCMD ${CMAKE_CURRENT_LIST_DIR}/settings/parseSettings.py)
add_custom_command(
    OUTPUT ${RSETTINGS} ${USETTINGS}
    COMMAND ${SETCMD} impl
    DEPENDS ${SETTINGSIN} ${SETCMD}
    VERBATIM
)
IF (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    if (NOT AVNAV_DEBUG_STANDALONE)
      set(TKHANDLER_SRC ${CMAKE_SOURCE_DIR}/tokenHandler/tokenHandlerDebug.js)
      set(TKHANDLER_ISRC ${CMAKE_SOURCE_DIR}/tokenHandler/publicKeyDebug.h)
    else(NOT AVNAV_DEBUG_STANDALONE)
      set(TKHANDLER_SRC ${CMAKE_SOURCE_DIR}/tokenHandler/tokenHandler.js)
      set(TKHANDLER_ISRC ${CMAKE_SOURCE_DIR}/tokenHandler/publicKey.h)
    endif(NOT AVNAV_DEBUG_STANDALONE)
ELSE(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(TKHANDLER_SRC ${CMAKE_SOURCE_DIR}/tokenHandler/tokenHandler.js)
    set(TKHANDLER_ISRC ${CMAKE_SOURCE_DIR}/tokenHandler/publicKey.h)
ENDIF(${CMAKE_BUILD_TYPE} MATCHES "Debug")
set(TKHANDLER_INCLUDE ${GENERATED_DIR}/publicKey.h)
add_custom_command(
    OUTPUT ${TKHANDLER_INCLUDE}
    COMMAND ${CMAKE_COMMAND} -E copy
    ${TKHANDLER_ISRC}
    ${TKHANDLER_INCLUDE}
)

list(APPEND SRC ${OBJECT_CLASSES} ${ATTRIDS} ${ATTRVALS} ${RSETTINGS} ${USETTINGS} ${TKHANDLER_INCLUDE} )

# add the executable
if (NOT AVNAV_ANDROID)
    add_executable(${TARGET} ${SRC} src/main.cpp ${HEADERS})
else()
    if (NOT ANDROID_BUILD_EXE)
        message("building android library")
        add_library(${TARGET} SHARED    src/android.cpp ${SRC} src/main.cpp ${HEADERS})
    else()
        message("building android exe")
        add_executable(${TARGET} ${SRC} src/main.cpp ${HEADERS})
        set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "${ANDROID_EXE}")
    endif()
endif()
target_compile_definitions(${TARGET} PUBLIC AVNAV_VERSION=${AVNAV_VERSION})
target_compile_definitions(${TARGET} PUBLIC OCHARTS_VERSION=${OCHARTS_VERSION})
target_include_directories(${TARGET} PRIVATE include include/requestHandler include/s52 src/HTTPd include/generated)
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    message("debug build")
    target_compile_definitions(${TARGET} PRIVATE AVDEBUG=1)
endif()


#fpng
add_subdirectory(lib/fpng)
target_compile_definitions(${TARGET} PUBLIC -DUSE_FPNG)
target_link_libraries(${TARGET} PRIVATE fpng)
#spng
add_subdirectory(lib/spng)
target_compile_definitions(${TARGET} PUBLIC -DUSE_SPNG)
target_link_libraries(${TARGET} PRIVATE spng)

#tinyxml
add_subdirectory(lib/tinyxml2)
target_link_libraries(${TARGET} PRIVATE tinyxml2)

#miniz
add_subdirectory(lib/miniz)
target_link_libraries(${TARGET} PRIVATE miniz)

#fastcsv
target_include_directories(${TARGET} PRIVATE lib/fastcsv)

#stb
target_include_directories(${TARGET} PRIVATE lib/stb)

#threads
if (NOT AVNAV_ANDROID)
    message("add pthread")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(${TARGET} PRIVATE Threads::Threads)
else()
    message("avnav android")
endif()

#curl, openssl
if (NOT AVNAV_ANDROID)
    message("curl...")
    find_package(CURL REQUIRED)
    target_link_libraries(${TARGET} PRIVATE ${CURL_LIBRARIES})
    target_include_directories(${TARGET} PRIVATE ${CURL_INCLUDE_DIRS})
    message("openssl...")
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${TARGET} PRIVATE OpenSSL::Crypto )
else()
    target_compile_definitions(${TARGET} PUBLIC -DAVNAV_ANDROID)
    message("curl...")
    find_package(curl REQUIRED CONFIG)
    target_link_libraries(${TARGET} PRIVATE curl::curl )
    target_include_directories(${TARGET} PRIVATE curl::curl)
    target_link_libraries(${TARGET} PRIVATE z)
    message("openssl...")
    #find_package(OpenSSL REQUIRED CONFIG) #already included by curl
    target_link_libraries(${TARGET} PRIVATE openssl::crypto )
endif()

#filesystem
target_include_directories(${TARGET} PRIVATE lib/filesystem)

#simplejson
target_include_directories(${TARGET} PRIVATE lib/simplejson)
target_sources(${TARGET} PRIVATE lib/simplejson/json.cpp)


#add oexserverd
if (AVNAV_ANDROID)
    add_custom_command(
        TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_LIST_DIR}/external/o-charts_pi/libs/oeserverd/oeserverda_android/oexserverd
        ${CMAKE_CURRENT_LIST_DIR}
        )
else()
    #testing only
    #TODO: use CMAKE_HOST_SYSTEM_PROCESSOR to determine the best oexserverd
    add_custom_command(
        TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_LIST_DIR}/external/o-charts_pi/libs/oeserverd/linux64/oexserverd
        ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

#preload
if (NOT AVNAV_ANDROID)
    add_library(preload SHARED preload/preload.cpp)
    target_include_directories(preload PRIVATE include)
    target_link_libraries(preload PRIVATE dl Threads::Threads)
endif()

#coordinate converted
if (NOT AVNAV_ANDROID)
    add_executable(chelper tools/chelp.cpp src/StringHelper.cpp)
    target_include_directories(chelper PRIVATE include)
    target_link_libraries(chelper PRIVATE ${CURL_LIBRARIES})
    target_include_directories(chelper PRIVATE ${CURL_INCLUDE_DIRS})
endif()

if(NOT NO_TEST)
#testing with googletest

FetchContent_Declare(
    googletest
    URL  https://github.com/google/googletest/archive/3026483ae575e2de942db5e760cf95e973308dd5.zip
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set (TEST_SRC test/TestHelper.cpp
    test/TFileHelper.cpp
    test/TStringHelper.cpp
    test/TChartInfo.cpp
    test/TChartCache.cpp
    test/TException.cpp
    test/TCoordinates.cpp
    test/TRuleCreator.cpp
    test/TS52Attributes.cpp
    test/TDrawingContext.cpp
    test/TAllocator.cpp
    )
add_executable(
  avtest
  ${TEST_SRC}
  ${SRC}
)
target_link_libraries(
  avtest
  GTest::gtest_main
)
target_compile_definitions(avtest PRIVATE -DAVTEST)
get_target_property(TEST_LIBS ${TARGET} LINK_LIBRARIES)
message("libs: ${TEST_LIBS}")
target_link_libraries(avtest ${TEST_LIBS})
get_target_property(TEST_INCLUDES ${TARGET} INCLUDE_DIRECTORIES)
message("include dirs: ${TEST_INCLUDES}")
target_include_directories(avtest PRIVATE  ${TEST_INCLUDES})
include(GoogleTest)
gtest_discover_tests(avtest)
endif()