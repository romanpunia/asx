cmake_minimum_required(VERSION 3.6)

#Setup toolchain file if needed
set(VI_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/mavi CACHE STRING "Mavi directory")
set(VI_USE_ASSIMP OFF CACHE BOOL "header-only")
set(VI_USE_FREETYPE OFF CACHE BOOL "header-only")
set(VI_USE_GLEW OFF CACHE BOOL "header-only")
set(VI_USE_MONGOC OFF CACHE BOOL "header-only")
set(VI_USE_POSTGRESQL OFF CACHE BOOL "header-only")
set(VI_USE_OPENAL OFF CACHE BOOL "header-only")
set(VI_USE_OPENGL OFF CACHE BOOL "header-only")
set(VI_USE_OPENSSL OFF CACHE BOOL "header-only")
set(VI_USE_SDL2 OFF CACHE BOOL "header-only")
set(VI_USE_ZLIB OFF CACHE BOOL "header-only")
set(VI_USE_SPIRV OFF CACHE BOOL "header-only")
set(VI_USE_SHADERS OFF CACHE BOOL "header-only")
set(VI_USE_FCTX OFF CACHE BOOL "header-only")
set(VI_USE_BULLET3 OFF CACHE BOOL "header-only")
set(VI_USE_RMLUI OFF CACHE BOOL "header-only")
set(VI_USE_BULLET3 OFF CACHE BOOL "header-only")

#Project's metadata
project(test_addon)

#Build directory if not defined
if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
endif()

#Add Mavi as dependency
add_subdirectory(${VI_DIRECTORY} mavi)
link_directories(${VI_DIRECTORY})

#Project's compile options
add_library(test_addon SHARED addon.cpp)
set_target_properties(test_addon PROPERTIES
        OUTPUT_NAME "test_addon"
		CXX_STANDARD ${VI_CXX}
        CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF)

#Link Mavi as dependency
target_include_directories(test_addon PRIVATE ${VI_DIRECTORY})
target_link_libraries(test_addon PRIVATE mavi)