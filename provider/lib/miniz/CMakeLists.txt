project(miniz)
cmake_minimum_required(VERSION 3.18)

add_library(miniz miniz.c)
target_include_directories(miniz INTERFACE ${PROJECT_SOURCE_DIR})
target_compile_definitions(miniz PRIVATE _LARGE_FILE_SOURCE=1)
target_compile_definitions(miniz PRIVATE __USE_LARGEFILE64)