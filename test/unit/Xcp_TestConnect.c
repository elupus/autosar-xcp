#include "Xcp_Test.h"

static void Xcp_TestConnectRepliesWithSlaveProperties(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* resources: DAQ|STIM, comm mode: little endian + block mode,
     * MAX_CTO 8, MAX_DTO 8, protocol/transport layer version 1 */
    XCP_EXCHANGE((XCP_PID_CMD_STD_CONNECT, 0x00),
                 0xFF, 0x0C, 0x40, 0x08, 0x08, 0x00, 0x10, 0x10);
    assert_int_equal(Xcp_TestTxPduId(0), XCP_PDU_ID_TX);
    assert_int_equal(Xcp_Connected, 1);
}

static void Xcp_TestConnectWithUnknownModeIsRejected(void** state)
{
    (void)state;
    Xcp_TestInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_CONNECT, 0x07), XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
    assert_int_equal(Xcp_Connected, 0);
}

static void Xcp_TestCommandsAreIgnoredBeforeConnect(void** state)
{
    (void)state;
    Xcp_TestInit();

    XCP_SEND(XCP_PID_CMD_STD_GET_STATUS);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 0);
}

static void Xcp_TestUnknownCommandGivesError(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((0xF9), XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
}

static void Xcp_TestDisconnect(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_DISCONNECT), XCP_PID_RES);
    assert_int_equal(Xcp_Connected, 0);
}

static void Xcp_TestGetStatusIdle(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS),
                 XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void Xcp_TestRxBeforeInitReportsDetError(void** state)
{
    (void)state;
    /* module deliberately not initialized */
    XCP_SEND(XCP_PID_CMD_STD_CONNECT, 0x00);
    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_NOT_INITIALIZED);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestConnectRepliesWithSlaveProperties),
        cmocka_unit_test(Xcp_TestConnectWithUnknownModeIsRejected),
        cmocka_unit_test(Xcp_TestCommandsAreIgnoredBeforeConnect),
        cmocka_unit_test(Xcp_TestUnknownCommandGivesError),
        cmocka_unit_test(Xcp_TestDisconnect),
        cmocka_unit_test(Xcp_TestGetStatusIdle),
        cmocka_unit_test(Xcp_TestRxBeforeInitReportsDetError),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
