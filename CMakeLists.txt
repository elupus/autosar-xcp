cmake_minimum_required(VERSION 3.14)
project(autosar-xcp LANGUAGES C)

add_subdirectory(source)

option(XCP_BUILD_TESTS "Build the host unit tests" ON)

if(XCP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
