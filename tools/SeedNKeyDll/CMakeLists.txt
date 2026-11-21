cmake_minimum_required(VERSION 3.10)
project(SeedNKeyDll LANGUAGES CXX)

# Seed&Key DLL loaded by XCP master tools (e.g. CANape, INCA).
#
# Most of those tools are 32-bit and can only load 32-bit DLLs. With Visual
# Studio generators select that explicitly:
#
#   cmake -S . -B build -A Win32
#   cmake --build build --config Release
#
# MinGW cross compile:
#
#   cmake -S . -B build -DCMAKE_SYSTEM_NAME=Windows \
#         -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++

if(NOT WIN32)
    message(FATAL_ERROR "SeedNKeyDll uses the Windows API and can only be built for a Windows target")
endif()

add_library(SeedNKeyDll SHARED
    seedNKeyXcp.h
    seedNKeyXcpMain.cpp
)

target_compile_definitions(SeedNKeyDll PRIVATE
    SEEDNKEYDLL_EXPORTS
    _USRDLL
    UNICODE
    _UNICODE
)

# Tools look the DLL up by file name, so avoid the "lib" prefix MinGW adds.
set_target_properties(SeedNKeyDll PROPERTIES PREFIX "")

if(MSVC)
    target_compile_options(SeedNKeyDll PRIVATE /W3)
else()
    target_compile_options(SeedNKeyDll PRIVATE -Wall -Wextra)
endif()
