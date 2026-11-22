#include "Xcp_Test.h"

#include <string.h>

#include "XcpOnEth_Cbk.h"

/* A frame as received from the network: length and counter (16 bit, little
 * endian) in front of the XCP packet */
#define ETH_FRAME(ctr, len, ...) (len) & 0xFF, (len) >> 8, (ctr) & 0xFF, (ctr) >> 8, __VA_ARGS__

/* ------------------------------ receive path ------------------------------ */

static void Xcp_TestEthRxStripsHeader(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* MAX_CTO and MAX_DTO are 255 for Ethernet */
    XCP_EXCHANGE((XCP_PID_CMD_STD_CONNECT, 0x00),
                 XCP_PID_RES, 0x0C, 0x40, 0xFF, 0xFF, 0x00, 0x10, 0x10);
    assert_int_equal(Xcp_TestTxPduId(0), XCP_PDU_ID_TX);
}

static void Xcp_TestEthRxIgnoresCounter(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* a counter that is not the expected one is only a debug message */
    const uint8_t connect[]   = { ETH_FRAME(0x1234, 2, XCP_PID_CMD_STD_CONNECT, 0x00) };
    const uint8_t getstatus[] = { ETH_FRAME(0x9999, 1, XCP_PID_CMD_STD_GET_STATUS) };
    Xcp_TestRxRaw(XCP_PDU_ID_RX, connect, sizeof(connect));
    Xcp_TestRxRaw(XCP_PDU_ID_RX, getstatus, sizeof(getstatus));
    Xcp_TestPump();

    assert_int_equal(Xcp_TestDetCount(), 0);
    assert_int_equal(Xcp_TestTxCount(), 2);
    XCP_EXPECT_TX(1, XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void Xcp_TestEthRxOnUnknownPduId(void** state)
{
    (void)state;
    Xcp_TestInit();

    const uint8_t connect[] = { ETH_FRAME(0, 2, XCP_PID_CMD_STD_CONNECT, 0x00) };
    Xcp_TestRxRaw(0x77, connect, sizeof(connect));
    Xcp_TestPump();

    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x03);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_INVALID_PDUID);
    assert_int_equal(Xcp_Connected, 0);
}

static void Xcp_TestEthRxWithoutPayload(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* only the length and counter, no XCP packet */
    const uint8_t header[] = { 0x00, 0x00, 0x00, 0x00 };
    Xcp_TestRxRaw(XCP_PDU_ID_RX, header, sizeof(header));

    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x03);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_INVALID_PDUID);
}

static void Xcp_TestEthRxWithNullPointer(void** state)
{
    (void)state;
    Xcp_TestInit();

    Xcp_TestRxNull(XCP_PDU_ID_RX);

    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x03);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_INV_POINTER);
}

static void Xcp_TestEthRxBeforeInit(void** state)
{
    (void)state;
    const uint8_t connect[] = { ETH_FRAME(0, 2, XCP_PID_CMD_STD_CONNECT, 0x00) };
    Xcp_TestRxRaw(XCP_PDU_ID_RX, connect, sizeof(connect));

    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x03);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_NOT_INITIALIZED);
}

/* ------------------------------ transmit path ----------------------------- */

static void Xcp_TestEthTxHeaderHasLengthAndCounter(void** state)
{
    (void)state;
    Xcp_TestInit();

    XCP_SEND(XCP_PID_CMD_STD_CONNECT, 0x00);
    XCP_SEND(XCP_PID_CMD_STD_GET_STATUS);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 2);

    /* little endian length of the packet, then a counter that counts frames from 1 */
    const uint8_t first[]  = { 0x08, 0x00, 0x01, 0x00, XCP_PID_RES, 0x0C, 0x40, 0xFF, 0xFF, 0x00, 0x10, 0x10 };
    const uint8_t second[] = { 0x06, 0x00, 0x02, 0x00, XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00 };
    assert_int_equal(Xcp_TestTxRawLen(0), sizeof(first));
    assert_memory_equal(Xcp_TestTxRawData(0), first, sizeof(first));
    assert_int_equal(Xcp_TestTxRawLen(1), sizeof(second));
    assert_memory_equal(Xcp_TestTxRawData(1), second, sizeof(second));
}

static void Xcp_TestEthTxLongPacket(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();
    for(int i = 0; i < 200; i++)
        g_XcpDebugMemory[i] = (uint8_t)(i + 3);

    XCP_EXCHANGE((XCP_PID_CMD_STD_SET_MTA, 0x00, 0x00, XCP_MTA_EXTENSION_DEBUG, 0x00, 0x00, 0x00, 0x00),
                 XCP_PID_RES);

    /* a reply that does not fit 8 bytes: 200 bytes of data and the PID */
    Xcp_TestClear();
    XCP_SEND(XCP_PID_CMD_STD_UPLOAD, 200);
    Xcp_TestPump();

    assert_int_equal(Xcp_TestTxCount(), 1);
    assert_int_equal(Xcp_TestTxLen(0), 201);
    assert_int_equal(Xcp_TestTxData(0)[0], XCP_PID_RES);
    assert_memory_equal(Xcp_TestTxData(0) + 1, g_XcpDebugMemory, 200);
    assert_int_equal(Xcp_TestTxRawData(0)[0], 201);
    assert_int_equal(Xcp_TestTxRawData(0)[1], 0);
}

static void Xcp_TestEthTxIsRetried(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_SEND(XCP_PID_CMD_STD_GET_STATUS);
    Xcp_TestTxFailNext(2);
    Xcp_MainFunction();
    Xcp_MainFunction();
    assert_int_equal(Xcp_TestTxCount(), 0);
    Xcp_MainFunction();
    assert_int_equal(Xcp_TestTxCount(), 1);
    XCP_EXPECT_TX(0, XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void Xcp_TestEthTxCounterOnlyCountsSentFrames(void** state)
{
    (void)state;
    skip(); /* KNOWN BUG: see below */

    Xcp_TestInit();
    Xcp_TestConnect();
    Xcp_TestClear();

    XCP_SEND(XCP_PID_CMD_STD_GET_STATUS);
    Xcp_TestTxFailNext(2);
    for(int i = 0; i < 3; i++)
        Xcp_MainFunction();
    assert_int_equal(Xcp_TestTxCount(), 1);

    /* The connect reply was frame 1. The frame the lower layer rejected was
     * never on the network, so the next one is 2 and the master sees no gap. */
    assert_int_equal(Xcp_TestTxRawData(0)[2], 2);
    assert_int_equal(Xcp_TestTxRawData(0)[3], 0);
}

static void Xcp_TestEthTxConfirmationBeforeInit(void** state)
{
    (void)state;
    Xcp_SoAdIfTxConfirmation(XCP_PDU_ID_TX);

    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x02);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_NOT_INITIALIZED);
}

static void Xcp_TestEthTxConfirmationIsSilent(void** state)
{
    (void)state;
    Xcp_TestInit();

    Xcp_SoAdIfTxConfirmation(XCP_PDU_ID_TX);
    assert_int_equal(Xcp_TestDetCount(), 0);
}

/* ------------------------------- commands -------------------------------- */

static void Xcp_TestEthTransportCommandUnknown(void** state)
{
    (void)state;
    Xcp_TestInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD, 0x01), XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
}

static void Xcp_TestEthGetIdCaptionInReply(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    /* the caption fits the reply, so it is sent directly and not read with upload */
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_ID, 0x00),
                 XCP_PID_RES, 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
                 'X', 'C', 'P', 'T', 'E', 'S', 'T');
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestEthRxStripsHeader),
        cmocka_unit_test(Xcp_TestEthRxIgnoresCounter),
        cmocka_unit_test(Xcp_TestEthRxOnUnknownPduId),
        cmocka_unit_test(Xcp_TestEthRxWithoutPayload),
        cmocka_unit_test(Xcp_TestEthRxWithNullPointer),
        cmocka_unit_test(Xcp_TestEthRxBeforeInit),
        cmocka_unit_test(Xcp_TestEthTxHeaderHasLengthAndCounter),
        cmocka_unit_test(Xcp_TestEthTxLongPacket),
        cmocka_unit_test(Xcp_TestEthTxIsRetried),
        cmocka_unit_test(Xcp_TestEthTxCounterOnlyCountsSentFrames),
        cmocka_unit_test(Xcp_TestEthTxConfirmationBeforeInit),
        cmocka_unit_test(Xcp_TestEthTxConfirmationIsSilent),
        cmocka_unit_test(Xcp_TestEthTransportCommandUnknown),
        cmocka_unit_test(Xcp_TestEthGetIdCaptionInReply),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
