# Host unit tests. The real XCP sources are built against small stubs of the
# AUTOSAR modules (test/stubs) and a hand written configuration (test/cfg/<transport>),
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

# The module under test is built once per transport, against the test stubs
# and the configuration in test/cfg/<transport>. The sources come from the xcp
# interface libraries. For each transport this gives:
#   xcp_under_test_<t>   the module
#   xcp_debug_check_<t>  the same sources with debug output enabled. XCP_DEBUG()
#                        discards its arguments when disabled, so this is what
#                        catches broken debug messages.
#   xcp_harness_<t>      the fake lower layer and helpers the tests use
function(xcp_add_transport transport)
    set(cfg ${CMAKE_CURRENT_SOURCE_DIR}/cfg/${transport})
    set(stubs ${CMAKE_CURRENT_SOURCE_DIR}/stubs)

    add_library(xcp_under_test_${transport} STATIC)
    target_link_libraries(xcp_under_test_${transport} PUBLIC xcp::core xcp::${transport})
    target_include_directories(xcp_under_test_${transport} PUBLIC ${cfg} ${stubs})
    target_compile_options(xcp_under_test_${transport} PRIVATE -Wall -Wextra -Wno-unused-parameter)

    add_library(xcp_debug_check_${transport} OBJECT)
    target_link_libraries(xcp_debug_check_${transport} PRIVATE xcp::core xcp::${transport})
    target_include_directories(xcp_debug_check_${transport} PRIVATE ${cfg} ${stubs})
    target_compile_definitions(xcp_debug_check_${transport} PRIVATE XCP_DEBUG_FN=printf)
    target_compile_options(xcp_debug_check_${transport} PRIVATE -include stdio.h
        -Wformat=2 -Werror=format -Werror=implicit-function-declaration -Werror=implicit-int)

    add_library(xcp_harness_${transport} STATIC harness/Xcp_Test.c)
    target_include_directories(xcp_harness_${transport} PUBLIC harness)
    target_link_libraries(xcp_harness_${transport} PUBLIC xcp_under_test_${transport} cmocka::cmocka)
    target_compile_options(xcp_harness_${transport} PRIVATE -Wall -Wextra)

    if(XCP_TEST_SANITIZE)
        foreach(t xcp_under_test_${transport} xcp_harness_${transport})
            # Undefined behaviour is fatal. Misaligned access and division by
            # zero are silently accepted on some hosts but fault or trap on
            # others, and should fail the same way everywhere.
            target_compile_options(${t} PUBLIC -fsanitize=address,undefined
                -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
            target_link_options(${t} PUBLIC -fsanitize=address,undefined
                -fno-sanitize-recover=undefined)
        endforeach()
    endif()
endfunction()

xcp_add_transport(can)
xcp_add_transport(eth)

# xcp_add_test(<name> [eth]) builds unit/<name>.c against a transport, CAN by default
function(xcp_add_test name)
    set(transport can)
    if(ARGC GREATER 1)
        set(transport ${ARGV1})
    endif()
    add_executable(${name} unit/${name}.c)
    target_link_libraries(${name} PRIVATE xcp_harness_${transport})
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

xcp_add_test(Xcp_TestConnect)
xcp_add_test(Xcp_TestMemory)
xcp_add_test(Xcp_TestProtect)
xcp_add_test(Xcp_TestGeneral)
xcp_add_test(Xcp_TestDaq)
xcp_add_test(Xcp_TestByteStream)
xcp_add_test(Xcp_TestCan can)
xcp_add_test(Xcp_TestEth eth)
