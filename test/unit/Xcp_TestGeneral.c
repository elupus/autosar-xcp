#include "Xcp_Test.h"

#include <string.h>

/* What the user function was called with */
static unsigned       Xcp_TestUserCalls;
static uint8          Xcp_TestUserSub;
static int            Xcp_TestUserLen;
static uint8          Xcp_TestUserData[16];
static Std_ReturnType Xcp_TestUserResult = E_OK;

static Std_ReturnType Xcp_TestUserFn(uint8 sub, const uint8* data, int len)
{
    Xcp_TestUserCalls++;
    Xcp_TestUserSub = sub;
    Xcp_TestUserLen = len;
    if(len > 0 && len <= (int)sizeof(Xcp_TestUserData))
        memcpy(Xcp_TestUserData, data, len);
    return Xcp_TestUserResult;
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

static void Xcp_TestGeneralUserCommandGetsSubCommandAndParameters(void** state)
{
    (void)state;
    Xcp_TestInitWith(&Xcp_TestUserCfg);
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_USER_CMD, 0x7A, 0x01, 0x02, 0x03), XCP_PID_RES);

    assert_int_equal(Xcp_TestUserCalls, 1);
    assert_int_equal(Xcp_TestUserSub, 0x7A);
    assert_int_equal(Xcp_TestUserLen, 3);
    assert_memory_equal(Xcp_TestUserData, "\x01\x02\x03", 3);
}

static void Xcp_TestGeneralUserCommandWithoutParameters(void** state)
{
    (void)state;
    Xcp_TestInitWith(&Xcp_TestUserCfg);
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_USER_CMD, 0x05), XCP_PID_RES);

    assert_int_equal(Xcp_TestUserCalls, 1);
    assert_int_equal(Xcp_TestUserSub, 0x05);
    assert_int_equal(Xcp_TestUserLen, 0);
}

static void Xcp_TestGeneralUserCommandFailureGivesError(void** state)
{
    (void)state;
    Xcp_TestInitWith(&Xcp_TestUserCfg);
    Xcp_TestConnect();
    Xcp_TestUserResult = E_NOT_OK;

    XCP_EXCHANGE((XCP_PID_CMD_STD_USER_CMD, 0x01, 0x02), XCP_PID_ERR, XCP_ERR_GENERIC);
    assert_int_equal(Xcp_TestUserCalls, 1);
}

static void Xcp_TestGeneralUserCommandNeedsSubCommand(void** state)
{
    (void)state;
    Xcp_TestInitWith(&Xcp_TestUserCfg);
    Xcp_TestConnect();

    /* only the command code, no sub command */
    XCP_EXCHANGE((XCP_PID_CMD_STD_USER_CMD), XCP_PID_ERR, XCP_ERR_CMD_SYNTAX);
    assert_int_equal(Xcp_TestUserCalls, 0);
}

static void Xcp_TestGeneralShortCommandGivesSyntaxError(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_SET_MTA), XCP_PID_ERR, XCP_ERR_CMD_SYNTAX);
}

static void Xcp_TestGeneralShortCommandDoesNotDropQueuedCommands(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    /* both are received before the main function runs */
    XCP_SEND(XCP_PID_CMD_STD_SET_MTA);
    XCP_SEND(XCP_PID_CMD_STD_GET_STATUS);
    Xcp_TestPump();

    assert_int_equal(Xcp_TestTxCount(), 2);
    XCP_EXPECT_TX(0, XCP_PID_ERR, XCP_ERR_CMD_SYNTAX);
    XCP_EXPECT_TX(1, XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void Xcp_TestGeneralBadCommandsDoNotExhaustBuffers(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    /* SET_MTA needs 8 bytes, a bare PID is too short. The receive buffer
     * must still be released, or a handful of these drain the pool and
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
        cmocka_unit_test(Xcp_TestGeneralUserCommandGetsSubCommandAndParameters),
        cmocka_unit_test(Xcp_TestGeneralUserCommandWithoutParameters),
        cmocka_unit_test(Xcp_TestGeneralUserCommandFailureGivesError),
        cmocka_unit_test(Xcp_TestGeneralUserCommandNeedsSubCommand),
        cmocka_unit_test(Xcp_TestGeneralShortCommandGivesSyntaxError),
        cmocka_unit_test(Xcp_TestGeneralShortCommandDoesNotDropQueuedCommands),
        cmocka_unit_test(Xcp_TestGeneralBadCommandsDoNotExhaustBuffers),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
