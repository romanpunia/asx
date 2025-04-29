cmake_minimum_required(VERSION 3.10)

#Setup toolchain file if needed
set(VI_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/vengeance CACHE STRING "Vengeance directory")
include(${VI_DIRECTORY}/deps/vitex/deps/toolchain.cmake)

#Project's metadata
project(asx)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(VI_PESSIMISTIC ON CACHE BOOL "+")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(VI_VECTORCLASS OFF CACHE BOOL "vectorclass is unavailable")
else()
    set(VI_VECTORCLASS ON CACHE BOOL "+")
endif()

#Build directory if not defined
if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
endif()

# Append shaders into the sources list
set(BUFFER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/etc")
set(BUFFER_DATA "#ifndef HAS_CODE_BUNDLE\n#define HAS_CODE_BUNDLE\n#include <string>\n\nnamespace code_bundle\n{\n\tvoid foreach(void* context, void(*callback)(void*, const char*, const char*, unsigned))\n\t{\n\t\tif (!callback)\n\t\t\treturn;\n")
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/interface.hpp DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/etc/addon)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime.hpp DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/etc/executable)
file(GLOB_RECURSE BINARIES ${BUFFER_DIR}/*)
foreach(BINARY ${BINARIES})
    string(REPLACE "${BUFFER_DIR}" "" FILENAME ${BINARY})
    string(REPLACE "${BUFFER_DIR}/" "" FILENAME ${BINARY})
    string(REGEX REPLACE "\\.| |-" "_" VARNAME ${FILENAME})
    string(REPLACE "/" "_" VARNAME ${VARNAME})
    string(TOLOWER ${VARNAME} VARNAME)
    file(READ ${BINARY} FILEDATA)
    if (NOT FILEDATA STREQUAL "")
        string(LENGTH "${FILEDATA}" FILESIZE)
        if (FILESIZE GREATER 4096)
            set(FILEOFFSET 0)
            string(APPEND BUFFER_DATA "\n\t\tstd::string dc_${VARNAME};\n\t\tdc_${VARNAME}.reserve(${FILESIZE});")
            while (FILEOFFSET LESS FILESIZE)
                math(EXPR CHUNKSIZE "${FILESIZE}-${FILEOFFSET}")
                if (CHUNKSIZE GREATER 4096)
                    set(CHUNKSIZE 4096)
                    string(SUBSTRING "${FILEDATA}" "${FILEOFFSET}" "${CHUNKSIZE}" CHUNKDATA)
                else()
                    string(SUBSTRING "${FILEDATA}" "${FILEOFFSET}" "-1" CHUNKDATA)
                endif()
                string(REGEX REPLACE "\\\\" "\\\\\\\\" CHUNKDATA "${CHUNKDATA}")
                string(REGEX REPLACE "\t" "\\\\t" CHUNKDATA "${CHUNKDATA}")
                string(REGEX REPLACE "\r" "\\\\r" CHUNKDATA "${CHUNKDATA}")
                string(REGEX REPLACE "\n" "\\\\n" CHUNKDATA "${CHUNKDATA}")
                string(REGEX REPLACE "\"" "\\\\\"" CHUNKDATA "${CHUNKDATA}")
                string(APPEND BUFFER_DATA "\n\t\tdc_${VARNAME} += \"${CHUNKDATA}\";")
                math(EXPR FILEOFFSET "${FILEOFFSET}+${CHUNKSIZE}")
            endwhile()
            string(APPEND BUFFER_DATA "\n\t\tcallback(context, \"${FILENAME}\", dc_${VARNAME}.c_str(), (unsigned int)dc_${VARNAME}.size());\n")
        else()
            string(REGEX REPLACE "\\\\" "\\\\\\\\" FILEDATA "${FILEDATA}")
            string(REGEX REPLACE "\t" "\\\\t" FILEDATA "${FILEDATA}")
            string(REGEX REPLACE "\r" "\\\\r" FILEDATA "${FILEDATA}")
            string(REGEX REPLACE "\n" "\\\\n" FILEDATA "${FILEDATA}")
            string(REGEX REPLACE "\"" "\\\\\"" FILEDATA "${FILEDATA}")
            string(APPEND BUFFER_DATA "\n\t\tconst char* sc_${VARNAME} = \"${FILEDATA}\";\n\t\tcallback(context, \"${FILENAME}\", sc_${VARNAME}, ${FILESIZE});\n")
        endif()    
    endif()
endforeach()
string(APPEND BUFFER_DATA "\t}\n}\n#endif")
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/code.hpp "${BUFFER_DATA}")

#Project's compile options
add_executable(asx
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/code.hpp)
set_target_properties(asx PROPERTIES
    OUTPUT_NAME "asx"
    CXX_STANDARD ${VI_CXX}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION})
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "/ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE")
endif()
install(TARGETS asx DESTINATION bin)

#Add Vitex as dependency
add_subdirectory(${VI_DIRECTORY} vitex)
target_link_libraries(asx PRIVATE vitex)