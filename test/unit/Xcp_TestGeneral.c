#include "Xcp_Test.h"

static unsigned Xcp_TestUserCalls;

static Std_ReturnType Xcp_TestUserFn(void* data, int len)
{
    (void)data; (void)len;
    Xcp_TestUserCalls++;
    return E_OK;
}

static const Xcp_ConfigType Xcp_TestUserCfg = {
    .XcpUserFn = Xcp_TestUserFn,
};

static void Xcp_TestGeneralGetCommModeInfo(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_COMM_MODE_INFO),
                 XCP_PID_RES, 0x00, 0x01, 0x00, 0x07, 0x00, 0x07, 0x10);
}

static void Xcp_TestGeneralSyncReportsSynch(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_SYNCH), XCP_PID_ERR, XCP_ERR_CMD_SYNCH);
}

static void Xcp_TestGeneralGetIdCaption(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    /* The text does not fit the reply, so the length is returned and the
     * master fetches the text with an upload from the MTA */
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_ID, 0x00),
                 XCP_PID_RES, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00);
    XCP_EXCHANGE((XCP_PID_CMD_STD_UPLOAD, 0x07),
                 XCP_PID_RES, 'X', 'C', 'P', 'T', 'E', 'S', 'T');
}

static void Xcp_TestGeneralGetIdMissingText(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    /* no MC2 file configured gives length zero */
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_ID, 0x01),
                 XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void Xcp_TestGeneralUserCommandWithoutFunction(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_USER_CMD, 0x01, 0x02), XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
}

static void Xcp_TestGeneralUserCommandCallsFunction(void** state)
{
    (void)state;
    Xcp_TestInitWith(&Xcp_TestUserCfg);
    Xcp_TestConnect();

    XCP_SEND(XCP_PID_CMD_STD_USER_CMD, 0x01, 0x02);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestUserCalls, 1);
}

static void Xcp_TestGeneralBadCommandsDoNotExhaustBuffers(void** state)
{
    (void)state;
    skip(); /* KNOWN BUG: see below */

    Xcp_TestInit();
    Xcp_TestConnect();

    /* SET_MTA needs 8 bytes, a bare PID is too short. Xcp_Recieve_Main
     * returns from inside FIFO_FOR_READ for those, which skips the free
     * of the receive buffer. A handful of them drains the buffer pool and
     * the slave stops answering. */
    for(int i = 0; i < 2 * XCP_MAX_RXTX_QUEUE; i++) {
        XCP_SEND(XCP_PID_CMD_STD_SET_MTA);
        Xcp_TestPump();
    }

    Xcp_TestClear();
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS), XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestGeneralGetCommModeInfo),
        cmocka_unit_test(Xcp_TestGeneralSyncReportsSynch),
        cmocka_unit_test(Xcp_TestGeneralGetIdCaption),
        cmocka_unit_test(Xcp_TestGeneralGetIdMissingText),
        cmocka_unit_test(Xcp_TestGeneralUserCommandWithoutFunction),
        cmocka_unit_test(Xcp_TestGeneralUserCommandCallsFunction),
        cmocka_unit_test(Xcp_TestGeneralBadCommandsDoNotExhaustBuffers),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
