# Host unit tests. The real XCP sources are built against small stubs of the
# AUTOSAR modules (test/stubs) and a hand written configuration (test/cfg),
# so no AUTOSAR stack is needed.

option(XCP_TEST_SANITIZE "Build the tests with ASan and UBSan" ON)

find_package(cmocka CONFIG QUIET)
if(NOT TARGET cmocka::cmocka AND NOT TARGET cmocka)
    include(FetchContent)
    set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
    set(WITH_EXAMPLES   OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(cmocka
        GIT_REPOSITORY https://git.cryptomilk.org/projects/cmocka.git
        GIT_TAG        cmocka-1.1.7)
    FetchContent_MakeAvailable(cmocka)
endif()
if(NOT TARGET cmocka::cmocka)
    add_library(cmocka::cmocka ALIAS cmocka)
endif()

# The module under test, with CAN as transport. The sources come from the
# xcp interface libraries and are built here against the test stubs/config.
add_library(xcp_under_test STATIC)
target_link_libraries(xcp_under_test PUBLIC xcp::core xcp::can)
target_include_directories(xcp_under_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)
target_compile_options(xcp_under_test PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_library(xcp_harness STATIC harness/Xcp_Test.c)
target_include_directories(xcp_harness PUBLIC harness)
target_link_libraries(xcp_harness PUBLIC xcp_under_test cmocka::cmocka)
target_compile_options(xcp_harness PRIVATE -Wall -Wextra)

if(XCP_TEST_SANITIZE)
    foreach(t xcp_under_test xcp_harness)
        target_compile_options(${t} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${t} PUBLIC -fsanitize=address,undefined)
    endforeach()
endif()

function(xcp_add_test name)
    add_executable(${name} unit/${name}.c)
    target_link_libraries(${name} PRIVATE xcp_harness)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

xcp_add_test(Xcp_TestConnect)
