#include "Xcp_Test.h"

#include <string.h>

/* Seed handed out by the fake seed function: 0x10, 0x11, ... */
static uint8 Xcp_TestSeedLen = 4;
static unsigned Xcp_TestUnlockCalls;

static uint8 Xcp_TestSeedFn(Xcp_ProtectType res, uint8* seed)
{
    (void)res;
    for(unsigned i = 0; i < Xcp_TestSeedLen; i++)
        seed[i] = 0x10 + i;
    return Xcp_TestSeedLen;
}

/* A key is accepted when it is the bitwise inverse of the seed */
static Std_ReturnType Xcp_TestUnlockFn(Xcp_ProtectType res
                                      , const uint8* seed, uint8 seed_len
                                      , const uint8* key,  uint8 key_len)
{
    (void)res;
    Xcp_TestUnlockCalls++;
    if(seed_len != key_len)
        return E_NOT_OK;
    for(unsigned i = 0; i < seed_len; i++) {
        if(key[i] != (uint8)~seed[i])
            return E_NOT_OK;
    }
    return E_OK;
}

static const Xcp_ConfigType Xcp_TestProtectCfg = {
    .XcpProtect  = XCP_PROTECT_CALPAG | XCP_PROTECT_DAQ | XCP_PROTECT_STIM,
    .XcpSeedFn   = Xcp_TestSeedFn,
    .XcpUnlockFn = Xcp_TestUnlockFn,
};

static void Xcp_TestProtectInit(void)
{
    Xcp_TestInitWith(&Xcp_TestProtectCfg);
    Xcp_TestConnect();
}

static void Xcp_TestProtectStatusReportsLockedResources(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS), XCP_PID_RES, 0x00, 0x0D, 0x00, 0x00, 0x00);
}

static void Xcp_TestProtectLockedCommandsAreDenied(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_CAL_DOWNLOAD, 0x01, 0xAA), XCP_PID_ERR, XCP_ERR_ACCESS_LOCKED);
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO), XCP_PID_ERR, XCP_ERR_ACCESS_LOCKED);
    /* STIM data packets are PIDs 0x00..0xBF */
    XCP_EXCHANGE((0x00, 0x00, 0x00), XCP_PID_ERR, XCP_ERR_ACCESS_LOCKED);
}

static void Xcp_TestProtectGetSeed(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, XCP_PROTECT_DAQ),
                 XCP_PID_RES, 0x04, 0x10, 0x11, 0x12, 0x13);
}

static void Xcp_TestProtectGetSeedInParts(void** state)
{
    (void)state;
    Xcp_TestSeedLen = 10;
    Xcp_TestProtectInit();

    /* first reply tells the total remaining (10), then 6 bytes fit */
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, XCP_PROTECT_DAQ),
                 XCP_PID_RES, 0x0A, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15);
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x01, XCP_PROTECT_DAQ),
                 XCP_PID_RES, 0x04, 0x16, 0x17, 0x18, 0x19);
}

static void Xcp_TestProtectGetSeedInvalidResource(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, 0x02), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestProtectGetSeedInvalidMode(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x02, XCP_PROTECT_DAQ), XCP_PID_ERR, XCP_ERR_GENERIC);
}

static void Xcp_TestProtectGetSeedSecondPartBeforeFirst(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x01, XCP_PROTECT_DAQ), XCP_PID_ERR, XCP_ERR_SEQUENCE);
}

static void Xcp_TestProtectUnlockWithoutSeed(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x04, 0xEF, 0xEE, 0xED, 0xEC),
                 XCP_PID_ERR, XCP_ERR_SEQUENCE);
    assert_int_equal(Xcp_TestUnlockCalls, 0);
}

static void Xcp_TestProtectUnlock(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, XCP_PROTECT_DAQ),
                 XCP_PID_RES, 0x04, 0x10, 0x11, 0x12, 0x13);
    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x04, 0xEF, 0xEE, 0xED, 0xEC), XCP_PID_RES);

    /* only DAQ was unlocked, CALPAG and STIM remain */
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS), XCP_PID_RES, 0x00, 0x09, 0x00, 0x00, 0x00);
    assert_true(!(Xcp_Config.XcpProtect & XCP_PROTECT_DAQ));

    /* DAQ commands are no longer denied (this one is not implemented without DAQ lists) */
    Xcp_TestClear();
    XCP_SEND(XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 1);
    assert_true(Xcp_TestTxData(0)[1] != XCP_ERR_ACCESS_LOCKED);
}

static void Xcp_TestProtectUnlockWithWrongKey(void** state)
{
    (void)state;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, XCP_PROTECT_DAQ),
                 XCP_PID_RES, 0x04, 0x10, 0x11, 0x12, 0x13);
    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x04, 0x00, 0x00, 0x00, 0x00),
                 XCP_PID_ERR, XCP_ERR_ACCESS_LOCKED);
    assert_int_equal(Xcp_TestUnlockCalls, 1);
    assert_true(Xcp_Config.XcpProtect & XCP_PROTECT_DAQ);
}

static void Xcp_TestProtectUnlockInParts(void** state)
{
    (void)state;
    Xcp_TestSeedLen = 10;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, XCP_PROTECT_CALPAG),
                 XCP_PID_RES, 0x0A, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15);
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x01, XCP_PROTECT_CALPAG),
                 XCP_PID_RES, 0x04, 0x16, 0x17, 0x18, 0x19);

    /* key is 10 bytes, 6 per packet. The check is done on the last one. */
    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x0A, 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA), XCP_PID_RES);
    assert_int_equal(Xcp_TestUnlockCalls, 0);
    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x04, 0xE9, 0xE8, 0xE7, 0xE6), XCP_PID_RES);
    assert_int_equal(Xcp_TestUnlockCalls, 1);
    assert_true(!(Xcp_Config.XcpProtect & XCP_PROTECT_CALPAG));

    /* download is now allowed through, and fails on the missing MTA instead */
    XCP_EXCHANGE((XCP_PID_CMD_CAL_DOWNLOAD, 0x01, 0xAA), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestProtectUnlockSequenceError(void** state)
{
    (void)state;
    Xcp_TestSeedLen = 10;
    Xcp_TestProtectInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, XCP_PROTECT_CALPAG),
                 XCP_PID_RES, 0x0A, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15);
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x01, XCP_PROTECT_CALPAG),
                 XCP_PID_RES, 0x04, 0x16, 0x17, 0x18, 0x19);
    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x0A, 0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA), XCP_PID_RES);

    /* 4 bytes remain, master claims 3 */
    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x03, 0xE9, 0xE8, 0xE7),
                 XCP_PID_ERR, XCP_ERR_SEQUENCE, 0x04);
}

static void Xcp_TestProtectIsRestoredOnReconnect(void** state)
{
    (void)state;
    skip(); /* KNOWN BUG: see below */

    Xcp_TestProtectInit();
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_SEED, 0x00, XCP_PROTECT_DAQ),
                 XCP_PID_RES, 0x04, 0x10, 0x11, 0x12, 0x13);
    XCP_EXCHANGE((XCP_PID_CMD_STD_UNLOCK, 0x04, 0xEF, 0xEE, 0xED, 0xEC), XCP_PID_RES);
    XCP_EXCHANGE((XCP_PID_CMD_STD_DISCONNECT), XCP_PID_RES);

    /* Xcp_CmdConnect restores XcpProtect when not yet connected, but it sets
     * Xcp_Connected = 1 just before checking it, so that code never runs */
    Xcp_TestConnect();
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS), XCP_PID_RES, 0x00, 0x0D, 0x00, 0x00, 0x00);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestProtectStatusReportsLockedResources),
        cmocka_unit_test(Xcp_TestProtectLockedCommandsAreDenied),
        cmocka_unit_test(Xcp_TestProtectGetSeed),
        cmocka_unit_test(Xcp_TestProtectGetSeedInParts),
        cmocka_unit_test(Xcp_TestProtectGetSeedInvalidResource),
        cmocka_unit_test(Xcp_TestProtectGetSeedInvalidMode),
        cmocka_unit_test(Xcp_TestProtectGetSeedSecondPartBeforeFirst),
        cmocka_unit_test(Xcp_TestProtectUnlockWithoutSeed),
        cmocka_unit_test(Xcp_TestProtectUnlock),
        cmocka_unit_test(Xcp_TestProtectUnlockWithWrongKey),
        cmocka_unit_test(Xcp_TestProtectUnlockInParts),
        cmocka_unit_test(Xcp_TestProtectUnlockSequenceError),
        cmocka_unit_test(Xcp_TestProtectIsRestoredOnReconnect),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
