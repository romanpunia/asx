cmake_minimum_required(VERSION 3.6)

#Setup toolchain file if needed
set(VI_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../mavi CACHE STRING "Mavi directory")
set(VI_USE_FAST_MEMORY ON CACHE BOOL "Fast memory is mostly preferred")
set(VI_USE_SIMD ON CACHE BOOL "SIMD is preferred")
include(${VI_DIRECTORY}/lib/toolchain.cmake)

#Project's metadata
project(mavias)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

#Build directory if not defined
if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
endif()

#Project's compile options
add_executable(mavias ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp)
set_target_properties(mavias PROPERTIES
		OUTPUT_NAME "mavias"
		CXX_STANDARD ${VI_CXX}
        CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION})
target_compile_definitions(mavias PRIVATE -DVI_EXPORT)

#Inherit compiler options
include(${VI_DIRECTORY}/lib/compiler.cmake)
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
endif()

#Add Mavi as dependency
add_subdirectory(${VI_DIRECTORY} mavi)
link_directories(${VI_DIRECTORY})
target_include_directories(mavias PRIVATE ${VI_DIRECTORY})
target_link_libraries(mavias PRIVATE mavi)