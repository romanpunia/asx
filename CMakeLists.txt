cmake_minimum_required(VERSION 3.6)

#Setup toolchain file if needed
set(ED_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../edge CACHE STRING "Edge directory")
set(ED_USE_SIMD ON CACHE BOOL "SIMD is preferred")
include(${ED_DIRECTORY}/lib/toolchain.cmake)

#Project's metadata
project(edge_as)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

#Build directory if not defined
if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
endif()

#Project's compile options
add_executable(edge_as ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp)
set_target_properties(edge_as PROPERTIES
		OUTPUT_NAME "edged"
		CXX_STANDARD 17
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION})
target_compile_definitions(edge_as PRIVATE -DED_EXPORT)

#Inherit compiler options
include(${ED_DIRECTORY}/lib/compiler.cmake)

#Add Edge as dependency
add_subdirectory(${ED_DIRECTORY} edge)
link_directories(${ED_DIRECTORY})
target_include_directories(edge_as PRIVATE ${ED_DIRECTORY})
target_link_libraries(edge_as PRIVATE edge)