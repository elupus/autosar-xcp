#include "Xcp_Test.h"

#include <string.h>

/* Address of g_XcpDebugMemory[0x10], little endian as Xcp_GetU32 is native */
#define MTA_DEBUG(off) XCP_PID_CMD_STD_SET_MTA, 0x00, 0x00, XCP_MTA_EXTENSION_DEBUG, \
                       (off), 0x00, 0x00, 0x00

static void Xcp_TestMemorySetMtaReplies(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((MTA_DEBUG(0x10)), XCP_PID_RES);
}

static void Xcp_TestMemoryShortUpload(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    memcpy(g_XcpDebugMemory + 0x10, "\x01\x02\x03\x04", 4);

    XCP_EXCHANGE((XCP_PID_CMD_STD_SHORT_UPLOAD, 0x03, 0x00, XCP_MTA_EXTENSION_DEBUG, 0x11, 0x00, 0x00, 0x00),
                 XCP_PID_RES, 0x02, 0x03, 0x04);
}

static void Xcp_TestMemoryShortUploadTooLong(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    /* only XCP_MAX_CTO - 1 bytes fit in the reply */
    XCP_EXCHANGE((XCP_PID_CMD_STD_SHORT_UPLOAD, 0x08, 0x00, XCP_MTA_EXTENSION_DEBUG, 0x00, 0x00, 0x00, 0x00),
                 XCP_PID_ERR, XCP_ERR_CMD_SYNTAX);
}

static void Xcp_TestMemoryShortUploadInvalidExtension(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_SHORT_UPLOAD, 0x01, 0x00, 0x42, 0x00, 0x00, 0x00, 0x00),
                 XCP_PID_ERR, XCP_ERR_CMD_SYNTAX);
}

static void Xcp_TestMemoryUploadSinglePacket(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    memcpy(g_XcpDebugMemory + 0x20, "\xA1\xA2\xA3\xA4", 4);

    XCP_EXCHANGE((MTA_DEBUG(0x20)), XCP_PID_RES);
    XCP_EXCHANGE((XCP_PID_CMD_STD_UPLOAD, 0x03), XCP_PID_RES, 0xA1, 0xA2, 0xA3);

    /* MTA moved past what was uploaded */
    XCP_EXCHANGE((XCP_PID_CMD_STD_UPLOAD, 0x01), XCP_PID_RES, 0xA4);
}

static void Xcp_TestMemoryUploadBlockMode(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    for(int i = 0; i < 10; i++)
        g_XcpDebugMemory[0x30 + i] = 0xB0 + i;

    XCP_EXCHANGE((MTA_DEBUG(0x30)), XCP_PID_RES);

    /* 10 bytes does not fit one 8 byte CTO, expect 7 + 3 */
    Xcp_TestClear();
    XCP_SEND(XCP_PID_CMD_STD_UPLOAD, 0x0A);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 2);
    XCP_EXPECT_TX(0, XCP_PID_RES, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6);
    XCP_EXPECT_TX(1, XCP_PID_RES, 0xB7, 0xB8, 0xB9);
}

static void Xcp_TestMemoryDownloadSinglePacket(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    memset(g_XcpDebugMemory, 0, 64);

    XCP_EXCHANGE((MTA_DEBUG(0x08)), XCP_PID_RES);
    XCP_EXCHANGE((XCP_PID_CMD_CAL_DOWNLOAD, 0x04, 0xAA, 0xBB, 0xCC, 0xDD), XCP_PID_RES);

    const uint8_t expect[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x00 };
    assert_memory_equal(g_XcpDebugMemory + 0x07, expect, sizeof(expect));
}

static void Xcp_TestMemoryDownloadWithoutMta(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_CAL_DOWNLOAD, 0x01, 0xAA), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestMemoryDownloadBlockMode(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    memset(g_XcpDebugMemory, 0, 64);

    XCP_EXCHANGE((MTA_DEBUG(0x00)), XCP_PID_RES);

    /* 8 bytes: first packet carries 6, no reply until the last arrives */
    Xcp_TestClear();
    XCP_SEND(XCP_PID_CMD_CAL_DOWNLOAD, 0x08, 1, 2, 3, 4, 5, 6);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 0);

    XCP_EXCHANGE((XCP_PID_CMD_CAL_DOWNLOAD_NEXT, 0x02, 7, 8), XCP_PID_RES);

    const uint8_t expect[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    assert_memory_equal(g_XcpDebugMemory, expect, sizeof(expect));
}

static void Xcp_TestMemoryDownloadBlockModeSequenceError(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((MTA_DEBUG(0x00)), XCP_PID_RES);

    Xcp_TestClear();
    XCP_SEND(XCP_PID_CMD_CAL_DOWNLOAD, 0x08, 1, 2, 3, 4, 5, 6);
    Xcp_TestPump();

    /* 2 bytes remain, master claims 5. Error reply tells how many remain. */
    XCP_EXCHANGE((XCP_PID_CMD_CAL_DOWNLOAD_NEXT, 0x05, 7, 8),
                 XCP_PID_ERR, XCP_ERR_SEQUENCE, 0x02);
}

static void Xcp_TestMemoryBuildChecksum(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    memcpy(g_XcpDebugMemory + 0x40, "\x01\x02\x03\x04", 4);

    XCP_EXCHANGE((MTA_DEBUG(0x40)), XCP_PID_RES);
    XCP_EXCHANGE((XCP_PID_CMD_STD_BUILD_CHECKSUM, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00),
                 XCP_PID_RES, XCP_CHECKSUM_ADD_11, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00);
}

static void Xcp_TestMemoryBuildChecksumWraps(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    memcpy(g_XcpDebugMemory + 0x40, "\xFF\x02", 2);

    /* ADD_11 is a byte wide sum */
    XCP_EXCHANGE((MTA_DEBUG(0x40)), XCP_PID_RES);
    XCP_EXCHANGE((XCP_PID_CMD_STD_BUILD_CHECKSUM, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00),
                 XCP_PID_RES, XCP_CHECKSUM_ADD_11, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00);
}

static void Xcp_TestMemoryBuildChecksumWithoutMta(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_EXCHANGE((XCP_PID_CMD_STD_BUILD_CHECKSUM, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00),
                 XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestMemorySetMtaReplies),
        cmocka_unit_test(Xcp_TestMemoryShortUpload),
        cmocka_unit_test(Xcp_TestMemoryShortUploadTooLong),
        cmocka_unit_test(Xcp_TestMemoryShortUploadInvalidExtension),
        cmocka_unit_test(Xcp_TestMemoryUploadSinglePacket),
        cmocka_unit_test(Xcp_TestMemoryUploadBlockMode),
        cmocka_unit_test(Xcp_TestMemoryDownloadSinglePacket),
        cmocka_unit_test(Xcp_TestMemoryDownloadWithoutMta),
        cmocka_unit_test(Xcp_TestMemoryDownloadBlockMode),
        cmocka_unit_test(Xcp_TestMemoryDownloadBlockModeSequenceError),
        cmocka_unit_test(Xcp_TestMemoryBuildChecksum),
        cmocka_unit_test(Xcp_TestMemoryBuildChecksumWraps),
        cmocka_unit_test(Xcp_TestMemoryBuildChecksumWithoutMta),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
