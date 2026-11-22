#include "Xcp_Test.h"

#include "XcpOnCan_Cbk.h"

/* Transport layer command, GET_SLAVE_ID sub command: "XCP" pattern and mode */
#define SLAVE_ID(c0, c1, c2, mode) XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD, 0xFF, (c0), (c1), (c2), (mode)

#define CAN_ID_RX_BYTES 0x00, 0x01, 0x00, 0x00 /* XCP_CAN_ID_RX, 0x100 */

/* ------------------------------ receive path ------------------------------ */

static void Xcp_TestCanRxOnBroadcastPduId(void** state)
{
    (void)state;
    Xcp_TestInit();

    const uint8_t connect[] = { XCP_PID_CMD_STD_CONNECT, 0x00 };
    Xcp_TestRxRaw(XCP_PDU_ID_BROADCAST, connect, sizeof(connect));
    Xcp_TestPump();

    assert_int_equal(Xcp_Connected, 1);
    assert_int_equal(Xcp_TestTxCount(), 1);
    assert_int_equal(Xcp_TestDetCount(), 0);
}

static void Xcp_TestCanRxOnUnknownPduId(void** state)
{
    (void)state;
    Xcp_TestInit();

    const uint8_t connect[] = { XCP_PID_CMD_STD_CONNECT, 0x00 };
    Xcp_TestRxRaw(0x77, connect, sizeof(connect));
    Xcp_TestPump();

    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x03);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_INVALID_PDUID);
    assert_int_equal(Xcp_Connected, 0);
    assert_int_equal(Xcp_TestTxCount(), 0);
}

static void Xcp_TestCanRxWithNullPointer(void** state)
{
    (void)state;
    Xcp_TestInit();

    Xcp_TestRxNull(XCP_PDU_ID_RX);

    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x03);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_INV_POINTER);
}

static void Xcp_TestCanRxSpecialForwardsPacket(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* the AUTOSAR 3.0 callback with channel, data, length and CAN id */
    const uint8_t connect[] = { XCP_PID_CMD_STD_CONNECT, 0x00 };
    Xcp_CanIfRxSpecial(0, XCP_PDU_ID_RX, connect, sizeof(connect), 0x100);
    Xcp_TestPump();

    assert_int_equal(Xcp_Connected, 1);
    assert_int_equal(Xcp_TestTxCount(), 1);
}

/* ------------------------------ transmit path ----------------------------- */

static void Xcp_TestCanTxConfirmationIsSilent(void** state)
{
    (void)state;
    Xcp_TestInit();

    Xcp_CanIfTxConfirmation(XCP_PDU_ID_TX);
    assert_int_equal(Xcp_TestDetCount(), 0);
}

static void Xcp_TestCanTxConfirmationBeforeInit(void** state)
{
    (void)state;
    skip(); /* KNOWN BUG: see below */

    /* module is not initialized */
    Xcp_CanIfTxConfirmation(XCP_PDU_ID_TX);

    /* the only thing that can be wrong is that the module is not initialized */
    assert_int_equal(Xcp_TestDetCount(), 1);
    assert_int_equal(Xcp_TestDetLastApi(), 0x02);
    assert_int_equal(Xcp_TestDetLastError(), XCP_E_NOT_INITIALIZED);
}

static void Xcp_TestCanTxIsRetriedInOrder(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_SEND(XCP_PID_CMD_STD_GET_STATUS);
    XCP_SEND(XCP_PID_CMD_STD_GET_COMM_MODE_INFO);

    /* both commands are handled, the first reply is rejected by the lower layer */
    Xcp_TestTxFailNext(1);
    Xcp_MainFunction();
    assert_int_equal(Xcp_TestTxCount(), 0);

    /* and goes out first when it is accepted */
    Xcp_MainFunction();
    Xcp_MainFunction();
    assert_int_equal(Xcp_TestTxCount(), 2);
    XCP_EXPECT_TX(0, XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
    XCP_EXPECT_TX(1, XCP_PID_RES, 0x00, 0x01, 0x00, 0x07, 0x00, 0x07, 0x10);
}

static void Xcp_TestCanTxIsRetriedUntilAccepted(void** state)
{
    (void)state;
    Xcp_TestInit();
    Xcp_TestConnect();

    XCP_SEND(XCP_PID_CMD_STD_GET_STATUS);
    Xcp_TestTxFailNext(3);
    for(int i = 0; i < 3; i++) {
        Xcp_MainFunction();
        assert_int_equal(Xcp_TestTxCount(), 0);
    }
    Xcp_MainFunction();
    assert_int_equal(Xcp_TestTxCount(), 1);
    XCP_EXPECT_TX(0, XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
}

/* ------------------------- transport layer commands ----------------------- */

static void Xcp_TestCanTransportCommandUnknown(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* transport layer commands are accepted before connect */
    XCP_EXCHANGE((XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD, 0x01), XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
}

static void Xcp_TestCanGetSlaveId(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* answered without being connected, with the CAN id the slave listens on */
    Xcp_TestClear();
    XCP_SEND(SLAVE_ID('X', 'C', 'P', 0x00));
    Xcp_TestPump();
    assert_true(Xcp_TestTxCount() >= 1);
    XCP_EXPECT_TX(0, XCP_PID_RES, 'X', 'C', 'P', CAN_ID_RX_BYTES);
    assert_int_equal(Xcp_Connected, 0);
}

static void Xcp_TestCanGetSlaveIdInverted(void** state)
{
    (void)state;
    Xcp_TestInit();

    Xcp_TestClear();
    XCP_SEND(SLAVE_ID('X', 'C', 'P', 0x01));
    Xcp_TestPump();
    assert_true(Xcp_TestTxCount() >= 1);
    XCP_EXPECT_TX(0, XCP_PID_RES, (uint8_t)~'X', (uint8_t)~'C', (uint8_t)~'P', CAN_ID_RX_BYTES);
}

static void Xcp_TestCanGetSlaveIdSingleReply(void** state)
{
    (void)state;
    skip(); /* KNOWN BUG: see below */

    Xcp_TestInit();

    /* one command gives one response. The data reply is followed by a
     * second, empty, positive response. */
    XCP_EXCHANGE((SLAVE_ID('X', 'C', 'P', 0x00)), XCP_PID_RES, 'X', 'C', 'P', CAN_ID_RX_BYTES);
}

static void Xcp_TestCanGetSlaveIdWrongPattern(void** state)
{
    (void)state;
    Xcp_TestInit();

    XCP_EXCHANGE((SLAVE_ID('X', 'C', 'Q', 0x00)), XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
}

static void Xcp_TestCanGetSlaveIdInvalidMode(void** state)
{
    (void)state;
    Xcp_TestInit();

    XCP_EXCHANGE((SLAVE_ID('X', 'C', 'P', 0x02)), XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
}

static void Xcp_TestCanGetSlaveIdTooShort(void** state)
{
    (void)state;
    Xcp_TestInit();

    /* the mode byte is missing */
    XCP_EXCHANGE((XCP_PID_CMD_STD_TRANSPORT_LAYER_CMD, 0xFF, 'X', 'C', 'P'),
                 XCP_PID_ERR, XCP_ERR_CMD_UNKNOWN);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestCanRxOnBroadcastPduId),
        cmocka_unit_test(Xcp_TestCanRxOnUnknownPduId),
        cmocka_unit_test(Xcp_TestCanRxWithNullPointer),
        cmocka_unit_test(Xcp_TestCanRxSpecialForwardsPacket),
        cmocka_unit_test(Xcp_TestCanTxConfirmationIsSilent),
        cmocka_unit_test(Xcp_TestCanTxConfirmationBeforeInit),
        cmocka_unit_test(Xcp_TestCanTxIsRetriedInOrder),
        cmocka_unit_test(Xcp_TestCanTxIsRetriedUntilAccepted),
        cmocka_unit_test(Xcp_TestCanTransportCommandUnknown),
        cmocka_unit_test(Xcp_TestCanGetSlaveId),
        cmocka_unit_test(Xcp_TestCanGetSlaveIdInverted),
        cmocka_unit_test(Xcp_TestCanGetSlaveIdSingleReply),
        cmocka_unit_test(Xcp_TestCanGetSlaveIdWrongPattern),
        cmocka_unit_test(Xcp_TestCanGetSlaveIdInvalidMode),
        cmocka_unit_test(Xcp_TestCanGetSlaveIdTooShort),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
