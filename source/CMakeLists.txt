# XCP module as CMake interface libraries.
#
# The module is configured by, and depends on headers from, the project that
# uses it (Xcp_Cfg.h, XcpOnCan_Cfg.h, Std_Types.h, ...). The sources are
# therefore compiled as part of the consumer target, which has to provide
# those headers on its include path.
#
#   target_link_libraries(my_ecu PRIVATE xcp::core xcp::can)
#
# xcp::core  Command handling, DAQ, memory access, programming
# xcp::can   CAN transport, CanIf callbacks (XcpOnCan.c)
# xcp::eth   Ethernet transport, SoAdIf callbacks (XcpOnEth.c)
#
# Link exactly one transport, each of them provides Xcp_Transmit().

add_library(xcp_core INTERFACE)
add_library(xcp::core ALIAS xcp_core)
target_include_directories(xcp_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(xcp_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Xcp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Xcp_Memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Xcp_Program.c
)

add_library(xcp_can INTERFACE)
add_library(xcp::can ALIAS xcp_can)
target_link_libraries(xcp_can INTERFACE xcp_core)
target_sources(xcp_can INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/XcpOnCan.c)

add_library(xcp_eth INTERFACE)
add_library(xcp::eth ALIAS xcp_eth)
target_link_libraries(xcp_eth INTERFACE xcp_core)
target_sources(xcp_eth INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/XcpOnEth.c)
