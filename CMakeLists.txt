cmake_minimum_required(VERSION 3.6)

#Setup toolchain file if needed
set(ED_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../edge CACHE STRING "Edge directory")
set(ED_USE_SIMD ON CACHE BOOL "SIMD is preferred")
include(${ED_DIRECTORY}/lib/toolchain.cmake)

#Project's metadata
project(edge_as)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")

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
		OUTPUT_NAME "edge"
		CXX_STANDARD 17
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION})
target_compile_definitions(edge_as PRIVATE -DED_EXPORT)

#Resolve needed options for different compilers and refs linkage
if (NOT MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if (NOT (WIN32 AND MINGW))
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat=0 -fexceptions")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration -fexceptions")
		if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
			else()
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
				set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
			endif()
		endif()
	else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_WINNT=0x0600")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WIN32_WINNT=0x0600")
    endif()
else()
    set(CMAKE_EXE_LINKER_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /MP")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG /MP")
    set(CMAKE_C_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /MP")
    set(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG /MP")
endif()

#Add Edge as dependency
add_subdirectory(${ED_DIRECTORY} edge)
link_directories(${ED_DIRECTORY})
target_include_directories(edge_as PRIVATE ${ED_DIRECTORY})
target_link_libraries(edge_as PRIVATE edge)