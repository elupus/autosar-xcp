#include "Xcp_Test.h"

#include <string.h>

/*
 * DAQ configuration used by the tests:
 *
 *   list 0  predefined (XcpMinDaq = 1), DAQ
 *   list 1  DAQ
 *   list 2  DAQ and STIM
 *
 * Every list has two ODTs with two entries each. There are two event
 * channels, "10ms" (0) and one without a name (1), both DAQ and STIM.
 */
#define DAQ_LISTS 3
#define DAQ_ODTS  2
#define DAQ_ENTRY 2

static Xcp_OdtEntryType Xcp_TestEntries[DAQ_LISTS][DAQ_ODTS][DAQ_ENTRY];
static Xcp_OdtType      Xcp_TestOdts   [DAQ_LISTS][DAQ_ODTS];
static Xcp_DaqListType  Xcp_TestLists  [DAQ_LISTS];

static Xcp_DaqListType* Xcp_TestChannel0Refs[DAQ_LISTS];
static Xcp_DaqListType* Xcp_TestChannel1Refs[DAQ_LISTS];

static Xcp_EventChannelType Xcp_TestChannels[2] = {
    {
        .XcpEventChannelNumber              = 0,
        .XcpEventChannelMaxDaqList          = DAQ_LISTS,
        .XcpEventChannelTriggeredDaqListRef = Xcp_TestChannel0Refs,
        .XcpEventChannelName                = "10ms",
        .XcpEventChannelProperties          = XCP_EVENTCHANNEL_PROPERTY_ALL,
        .XcpEventChannelUnit                = XCP_TIMESTAMP_UNIT_1MS,
        .XcpEventChannelRate                = 10,
    }, {
        .XcpEventChannelNumber              = 1,
        .XcpEventChannelMaxDaqList          = DAQ_LISTS,
        .XcpEventChannelTriggeredDaqListRef = Xcp_TestChannel1Refs,
        .XcpEventChannelProperties          = XCP_EVENTCHANNEL_PROPERTY_ALL,
    }
};

static const Xcp_ConfigType Xcp_TestDaqCfg = {
    .XcpDaqList         = Xcp_TestLists,
    .XcpEventChannel    = Xcp_TestChannels,
    .XcpMaxDaq          = DAQ_LISTS,
    .XcpMinDaq          = 1,
    .XcpMaxEventChannel = 2,
};

static void Xcp_TestDaqInit(void)
{
    for(int l = 0; l < DAQ_LISTS; l++) {
        Xcp_DaqListType* daq = &Xcp_TestLists[l];
        daq->XcpMaxOdt = DAQ_ODTS;
        daq->XcpOdtCount = DAQ_ODTS;
        daq->XcpOdt = Xcp_TestOdts[l];
        daq->XcpParams.Properties = XCP_DAQLIST_PROPERTY_DAQ;
        if(l == 2)
            daq->XcpParams.Properties |= XCP_DAQLIST_PROPERTY_STIM;
        for(int o = 0; o < DAQ_ODTS; o++) {
            Xcp_TestOdts[l][o].XcpMaxOdtEntries = DAQ_ENTRY;
            Xcp_TestOdts[l][o].XcpOdtEntry = Xcp_TestEntries[l][o];
        }
    }
    Xcp_TestInitWith(&Xcp_TestDaqCfg);
    Xcp_TestConnect();
}

#define DAQ_PTR(list, odt, ent) XCP_PID_CMD_DAQ_SET_DAQ_PTR, 0x00, (list), 0x00, (odt), (ent)
#define DAQ_WRITE(bit, size, addr) XCP_PID_CMD_DAQ_WRITE_DAQ, (bit), (size), \
                                   XCP_MTA_EXTENSION_DEBUG, (addr), 0x00, 0x00, 0x00
#define DAQ_MODE(mode, list, event, presc) XCP_PID_CMD_DAQ_SET_DAQ_LIST_MODE, (mode), \
                                   (list), 0x00, (event), 0x00, (presc), 0x00
#define DAQ_START(list) XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST, 0x01, (list), 0x00
#define NO_BIT 0xFF

/* ------------------------------ information ------------------------------ */

static void Xcp_TestDaqProcessorInfo(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* prescaler supported, static config, 3 lists (1 predefined), 2 events,
     * relative word identification */
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_PROCESSOR_INFO),
                 XCP_PID_RES, 0x02, 0x03, 0x00, 0x02, 0x00, 0x01, 0x80);
}

static void Xcp_TestDaqResolutionInfo(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* byte granularity, 5 bytes per entry (8 byte DTO minus 3 id bytes), no timestamp */
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_RESOLUTION_INFO),
                 XCP_PID_RES, 0x01, 0x05, 0x01, 0x05, 0x00, 0x00, 0x00);
}

static void Xcp_TestDaqClock(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_CLOCK),
                 XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void Xcp_TestDaqListInfo(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* properties, max odt, max odt entries, fixed event */
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO, 0x00, 0x00, 0x00),
                 XCP_PID_RES, XCP_DAQLIST_PROPERTY_DAQ | XCP_DAQLIST_PROPERTY_PREDEFINED,
                 DAQ_ODTS, 0x05, 0x00, 0x00);
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO, 0x00, 0x02, 0x00),
                 XCP_PID_RES, XCP_DAQLIST_PROPERTY_DAQ | XCP_DAQLIST_PROPERTY_STIM,
                 DAQ_ODTS, 0x05, 0x00, 0x00);
}

static void Xcp_TestDaqListInfoOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_LIST_INFO, 0x00, DAQ_LISTS, 0x00),
                 XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqEventInfo(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* properties, max daq lists, name length, rate, unit, priority */
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO, 0x00, 0x00, 0x00),
                 XCP_PID_RES, XCP_EVENTCHANNEL_PROPERTY_ALL, DAQ_LISTS, 0x04, 0x0A,
                 XCP_TIMESTAMP_UNIT_1MS, 0x00);
    /* the name is read with an upload */
    XCP_EXCHANGE((XCP_PID_CMD_STD_UPLOAD, 0x04), XCP_PID_RES, '1', '0', 'm', 's');

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO, 0x00, 0x01, 0x00),
                 XCP_PID_RES, XCP_EVENTCHANNEL_PROPERTY_ALL, DAQ_LISTS, 0x00, 0x00,
                 XCP_TIMESTAMP_UNIT_1NS, 0x00);
}

static void Xcp_TestDaqEventInfoOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_EVENT_INFO, 0x00, 0x02, 0x00),
                 XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

/* ------------------------------ configuration ----------------------------- */

static void Xcp_TestDaqSetPtr(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 1, 1)), XCP_PID_RES);
}

static void Xcp_TestDaqSetPtrOdtOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, DAQ_ODTS, 0)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqSetPtrEntryOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, DAQ_ENTRY)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqSetPtrListOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* must be rejected before the list is looked up */
    XCP_EXCHANGE((DAQ_PTR(DAQ_LISTS + 4, 0, 0)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqSetPtrEntryFarOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* same for entries, they are checked before walking the list */
    XCP_EXCHANGE((DAQ_PTR(1, 0, 200)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqWriteStoresEntry(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 1, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 2, 0x10)), XCP_PID_RES);

    const Xcp_OdtEntryType* e = &Xcp_TestEntries[1][1][0];
    assert_int_equal(e->XcpOdtEntryLength, 2);
    assert_int_equal(e->XcpOdtEntryExtension, XCP_MTA_EXTENSION_DEBUG);
    assert_int_equal(e->XcpOdtEntryAddress, 0x10);
    assert_int_equal(e->BitOffSet, 0xFF);
    assert_int_equal(Xcp_TestOdts[1][1].XcpOdtEntriesValid, 1);
}

static void Xcp_TestDaqWriteAdvancesPointer(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x10)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 3, 0x20)), XCP_PID_RES);

    assert_int_equal(Xcp_TestEntries[1][0][0].XcpOdtEntryAddress, 0x10);
    assert_int_equal(Xcp_TestEntries[1][0][1].XcpOdtEntryAddress, 0x20);
    assert_int_equal(Xcp_TestEntries[1][0][1].XcpOdtEntryLength, 3);
    assert_int_equal(Xcp_TestOdts[1][0].XcpOdtEntriesValid, 2);
}

static void Xcp_TestDaqWriteZeroSizeRemovesEntry(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 2, 0x10)), XCP_PID_RES);
    assert_int_equal(Xcp_TestOdts[1][0].XcpOdtEntriesValid, 1);

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 0, 0x10)), XCP_PID_RES);
    assert_int_equal(Xcp_TestOdts[1][0].XcpOdtEntriesValid, 0);
}

static void Xcp_TestDaqWriteTooLarge(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 6, 0x10)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqWriteBitOffsetNeedsGranularity(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    /* a bit offset is only valid for the granularity size, 1 byte here */
    XCP_EXCHANGE((DAQ_WRITE(3, 2, 0x10)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
    XCP_EXCHANGE((DAQ_WRITE(3, 1, 0x10)), XCP_PID_RES);
    assert_int_equal(Xcp_TestEntries[1][0][0].BitOffSet, 3);
}

static void Xcp_TestDaqWritePredefinedListIsProtected(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(0, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x10)), XCP_PID_ERR, XCP_ERR_WRITE_PROTECTED);
}

static void Xcp_TestDaqWriteWithoutPointer(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* no DAQ pointer has been set */
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x10)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqWritePastLastEntry(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x10)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x11)), XCP_PID_RES);
    /* the ODT is full, the pointer is gone */
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x12)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqReadDaq(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(3, 1, 0x10)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);

    /* READ_DAQ replies with a positive response, bit offset, size, extension, address */
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_READ_DAQ),
                 XCP_PID_RES, 0x03, 0x01, XCP_MTA_EXTENSION_DEBUG, 0x10, 0x00, 0x00, 0x00);
}

static void Xcp_TestDaqClearList(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 2, 0x10)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x20)), XCP_PID_RES);

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_CLEAR_DAQ_LIST, 0x00, 0x01, 0x00), XCP_PID_RES);

    for(int e = 0; e < DAQ_ENTRY; e++) {
        assert_int_equal(Xcp_TestEntries[1][0][e].XcpOdtEntryLength, 0);
        assert_int_equal(Xcp_TestEntries[1][0][e].XcpOdtEntryAddress, 0);
    }
    assert_int_equal(Xcp_TestOdts[1][0].XcpOdtEntriesValid, 0);
}

static void Xcp_TestDaqClearPredefinedList(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_CLEAR_DAQ_LIST, 0x00, 0x00, 0x00),
                 XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

/* --------------------------------- mode ---------------------------------- */

static void Xcp_TestDaqSetModeAssignsEventChannel(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 0, 1)), XCP_PID_RES);
    assert_int_equal(Xcp_TestChannels[0].XcpEventChannelDaqCount, 1);
    assert_ptr_equal(Xcp_TestChannel0Refs[0], &Xcp_TestLists[1]);

    /* mode, reserved, event channel, prescaler, priority */
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE, 0x00, 0x01, 0x00),
                 XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00);
}

static void Xcp_TestDaqSetModeMovesBetweenEventChannels(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 0, 1)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 1, 4)), XCP_PID_RES);
    assert_int_equal(Xcp_TestChannels[0].XcpEventChannelDaqCount, 0);
    assert_int_equal(Xcp_TestChannels[1].XcpEventChannelDaqCount, 1);

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE, 0x00, 0x01, 0x00),
                 XCP_PID_RES, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00);
}

static void Xcp_TestDaqSetModeKeepsTimestampBit(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_MODE(XCP_DAQLIST_MODE_TIMESTAMP, 1, 0, 1)), XCP_PID_RES);
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE, 0x00, 0x01, 0x00),
                 XCP_PID_RES, XCP_DAQLIST_MODE_TIMESTAMP, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00);
}

static void Xcp_TestDaqSetModeStimNeedsStimList(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_MODE(XCP_DAQLIST_MODE_STIM, 1, 0, 1)), XCP_PID_ERR, XCP_ERR_CMD_SYNTAX);
    XCP_EXCHANGE((DAQ_MODE(XCP_DAQLIST_MODE_STIM, 2, 0, 1)), XCP_PID_RES);
}

static void Xcp_TestDaqSetModePredefinedList(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_MODE(0x00, 0, 0, 1)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqSetModeListOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_MODE(0x00, DAQ_LISTS, 0, 1)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqSetModePriorityNotSupported(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_SET_DAQ_LIST_MODE, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05),
                 XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqSetModeEventChannelOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    /* there is no event channel 2 */
    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 2, 1)), XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqGetModeListOutOfRange(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_GET_DAQ_LIST_MODE, 0x00, DAQ_LISTS, 0x00),
                 XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

/* ------------------------------ start and stop ---------------------------- */

static void Xcp_TestDaqStartStop(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS), XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);

    /* the reply carries the PID of the first ODT */
    XCP_EXCHANGE((DAQ_START(1)), XCP_PID_RES, 0x00);
    assert_true(Xcp_TestLists[1].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS), XCP_PID_RES, 0x40, 0x00, 0x00, 0x00, 0x00);

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST, 0x00, 0x01, 0x00), XCP_PID_RES, 0x00);
    assert_false(Xcp_TestLists[1].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
    XCP_EXCHANGE((XCP_PID_CMD_STD_GET_STATUS), XCP_PID_RES, 0x00, 0x00, 0x00, 0x00, 0x00);
}

static void Xcp_TestDaqStartStopInvalid(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST, 0x03, 0x01, 0x00),
                 XCP_PID_ERR, XCP_ERR_MODE_NOT_VALID);
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST, 0x01, DAQ_LISTS, 0x00),
                 XCP_PID_ERR, XCP_ERR_OUT_OF_RANGE);
}

static void Xcp_TestDaqSynchStartsSelectedLists(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST, 0x02, 0x01, 0x00), XCP_PID_RES, 0x00);
    assert_false(Xcp_TestLists[1].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_SYNCH, 0x01), XCP_PID_RES);
    assert_true (Xcp_TestLists[1].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
    assert_false(Xcp_TestLists[1].XcpParams.Mode & XCP_DAQLIST_MODE_SELECTED);
    assert_false(Xcp_TestLists[2].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
}

static void Xcp_TestDaqSynchStopsSelectedLists(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_START(1)), XCP_PID_RES, 0x00);
    XCP_EXCHANGE((DAQ_START(2)), XCP_PID_RES, 0x00);
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST, 0x02, 0x01, 0x00), XCP_PID_RES, 0x00);

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_SYNCH, 0x02), XCP_PID_RES);
    assert_false(Xcp_TestLists[1].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
    assert_true (Xcp_TestLists[2].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
}

static void Xcp_TestDaqSynchStopsAllLists(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_START(1)), XCP_PID_RES, 0x00);
    XCP_EXCHANGE((DAQ_START(2)), XCP_PID_RES, 0x00);

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_SYNCH, 0x00), XCP_PID_RES);
    assert_false(Xcp_TestLists[1].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
    assert_false(Xcp_TestLists[2].XcpParams.Mode & XCP_DAQLIST_MODE_RUNNING);
}

static void Xcp_TestDaqSynchInvalidMode(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_SYNCH, 0x03), XCP_PID_ERR, XCP_ERR_MODE_NOT_VALID);
}

static void Xcp_TestDaqRunningListCannotBeChanged(void** state)
{
    (void)state;
    Xcp_TestDaqInit();

    XCP_EXCHANGE((DAQ_START(1)), XCP_PID_RES, 0x00);

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_ERR, XCP_ERR_DAQ_ACTIVE);
    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 0, 1)), XCP_PID_ERR, XCP_ERR_DAQ_ACTIVE);
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_CLEAR_DAQ_LIST, 0x00, 0x01, 0x00), XCP_PID_ERR, XCP_ERR_DAQ_ACTIVE);
}

/* ------------------------------ measurements ------------------------------ */

/* Configure list 1 with ODT 0: 2 bytes at 0x10 and 1 byte at 0x20, running on event 0 */
static void Xcp_TestDaqStartMeasurement(uint8_t prescaler)
{
    memcpy(g_XcpDebugMemory + 0x10, "\xA1\xA2", 2);
    memcpy(g_XcpDebugMemory + 0x20, "\xB1", 1);

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 2, 0x10)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x20)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 0, prescaler)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_START(1)), XCP_PID_RES, 0x00);
}

static void Xcp_TestDaqEventSendsDto(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    Xcp_TestDaqStartMeasurement(1);

    Xcp_TestClear();
    Xcp_MainFunction_Channel(0);
    Xcp_TestPump();

    /* ODT PID, DAQ list number (16 bit), then the sampled data */
    assert_int_equal(Xcp_TestTxCount(), 1);
    XCP_EXPECT_TX(0, 0x00, 0x01, 0x00, 0xA1, 0xA2, 0xB1);
    assert_int_equal(Xcp_TestTxPduId(0), XCP_PDU_ID_TX);
}

static void Xcp_TestDaqEventSamplesCurrentValues(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    Xcp_TestDaqStartMeasurement(1);

    Xcp_TestClear();
    Xcp_MainFunction_Channel(0);
    g_XcpDebugMemory[0x20] = 0xC7;
    Xcp_MainFunction_Channel(0);
    Xcp_TestPump();

    assert_int_equal(Xcp_TestTxCount(), 2);
    XCP_EXPECT_TX(0, 0x00, 0x01, 0x00, 0xA1, 0xA2, 0xB1);
    XCP_EXPECT_TX(1, 0x00, 0x01, 0x00, 0xA1, 0xA2, 0xC7);
}

static void Xcp_TestDaqEventOnOtherChannelSendsNothing(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    Xcp_TestDaqStartMeasurement(1);

    Xcp_TestClear();
    Xcp_MainFunction_Channel(1);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 0);
}

static void Xcp_TestDaqStoppedListSendsNothing(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    Xcp_TestDaqStartMeasurement(1);
    XCP_EXCHANGE((XCP_PID_CMD_DAQ_START_STOP_DAQ_LIST, 0x00, 0x01, 0x00), XCP_PID_RES, 0x00);

    Xcp_TestClear();
    Xcp_MainFunction_Channel(0);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 0);
}

static void Xcp_TestDaqDisconnectedSendsNothing(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    Xcp_TestDaqStartMeasurement(1);
    XCP_EXCHANGE((XCP_PID_CMD_STD_DISCONNECT), XCP_PID_RES);

    Xcp_TestClear();
    Xcp_MainFunction_Channel(0);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 0);
}

static void Xcp_TestDaqPrescaler(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    Xcp_TestDaqStartMeasurement(2);

    /* every second event is sampled: 1st and 3rd */
    Xcp_TestClear();
    for(int i = 0; i < 4; i++)
        Xcp_MainFunction_Channel(0);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 2);
}

static void Xcp_TestDaqPrescalerZeroIsEveryEvent(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    Xcp_TestDaqStartMeasurement(0);

    /* a prescaler of 0 is handled as 1 */
    Xcp_TestClear();
    Xcp_MainFunction_Channel(0);
    Xcp_TestPump();
    assert_int_equal(Xcp_TestTxCount(), 1);
}

static void Xcp_TestDaqOdtsHaveOwnPid(void** state)
{
    (void)state;
    skip(); /* KNOWN BUG: see below */

    Xcp_TestDaqInit();
    memcpy(g_XcpDebugMemory + 0x10, "\xA1", 1);
    memcpy(g_XcpDebugMemory + 0x20, "\xB1", 1);

    XCP_EXCHANGE((DAQ_PTR(1, 0, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x10)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 0, 0x10)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_PTR(1, 1, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x20)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 0, 0x20)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 0, 1)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_START(1)), XCP_PID_RES, 0x00);

    Xcp_TestClear();
    Xcp_MainFunction_Channel(0);
    Xcp_TestPump();

    /* with relative identification the PID is the ODT number in the list */
    assert_int_equal(Xcp_TestTxCount(), 2);
    XCP_EXPECT_TX(0, 0x00, 0x01, 0x00, 0xA1);
    XCP_EXPECT_TX(1, 0x01, 0x01, 0x00, 0xB1);
}

static void Xcp_TestDaqEmptyOdtDoesNotBlockLaterOdts(void** state)
{
    (void)state;
    Xcp_TestDaqInit();
    memcpy(g_XcpDebugMemory + 0x20, "\xB1", 1);

    /* ODT 0 is left empty, only ODT 1 has data */
    XCP_EXCHANGE((DAQ_PTR(1, 1, 0)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 1, 0x20)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_WRITE(NO_BIT, 0, 0x20)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_MODE(0x00, 1, 0, 1)), XCP_PID_RES);
    XCP_EXCHANGE((DAQ_START(1)), XCP_PID_RES, 0x00);

    Xcp_TestClear();
    Xcp_MainFunction_Channel(0);
    Xcp_TestPump();

    assert_int_equal(Xcp_TestTxCount(), 1);
    /* the PID of ODT 1, whatever the numbering is */
    XCP_EXPECT_TX(0, Xcp_TestOdts[1][1].XcpOdt2DtoMapping.XcpDtoPid, 0x01, 0x00, 0xB1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestDaqProcessorInfo),
        cmocka_unit_test(Xcp_TestDaqResolutionInfo),
        cmocka_unit_test(Xcp_TestDaqClock),
        cmocka_unit_test(Xcp_TestDaqListInfo),
        cmocka_unit_test(Xcp_TestDaqListInfoOutOfRange),
        cmocka_unit_test(Xcp_TestDaqEventInfo),
        cmocka_unit_test(Xcp_TestDaqEventInfoOutOfRange),

        cmocka_unit_test(Xcp_TestDaqSetPtr),
        cmocka_unit_test(Xcp_TestDaqSetPtrOdtOutOfRange),
        cmocka_unit_test(Xcp_TestDaqSetPtrEntryOutOfRange),
        cmocka_unit_test(Xcp_TestDaqSetPtrListOutOfRange),
        cmocka_unit_test(Xcp_TestDaqSetPtrEntryFarOutOfRange),
        cmocka_unit_test(Xcp_TestDaqWriteStoresEntry),
        cmocka_unit_test(Xcp_TestDaqWriteAdvancesPointer),
        cmocka_unit_test(Xcp_TestDaqWriteZeroSizeRemovesEntry),
        cmocka_unit_test(Xcp_TestDaqWriteTooLarge),
        cmocka_unit_test(Xcp_TestDaqWriteBitOffsetNeedsGranularity),
        cmocka_unit_test(Xcp_TestDaqWritePredefinedListIsProtected),
        cmocka_unit_test(Xcp_TestDaqWriteWithoutPointer),
        cmocka_unit_test(Xcp_TestDaqWritePastLastEntry),
        cmocka_unit_test(Xcp_TestDaqReadDaq),
        cmocka_unit_test(Xcp_TestDaqClearList),
        cmocka_unit_test(Xcp_TestDaqClearPredefinedList),

        cmocka_unit_test(Xcp_TestDaqSetModeAssignsEventChannel),
        cmocka_unit_test(Xcp_TestDaqSetModeMovesBetweenEventChannels),
        cmocka_unit_test(Xcp_TestDaqSetModeKeepsTimestampBit),
        cmocka_unit_test(Xcp_TestDaqSetModeStimNeedsStimList),
        cmocka_unit_test(Xcp_TestDaqSetModePredefinedList),
        cmocka_unit_test(Xcp_TestDaqSetModeListOutOfRange),
        cmocka_unit_test(Xcp_TestDaqSetModePriorityNotSupported),
        cmocka_unit_test(Xcp_TestDaqSetModeEventChannelOutOfRange),
        cmocka_unit_test(Xcp_TestDaqGetModeListOutOfRange),

        cmocka_unit_test(Xcp_TestDaqStartStop),
        cmocka_unit_test(Xcp_TestDaqStartStopInvalid),
        cmocka_unit_test(Xcp_TestDaqSynchStartsSelectedLists),
        cmocka_unit_test(Xcp_TestDaqSynchStopsSelectedLists),
        cmocka_unit_test(Xcp_TestDaqSynchStopsAllLists),
        cmocka_unit_test(Xcp_TestDaqSynchInvalidMode),
        cmocka_unit_test(Xcp_TestDaqRunningListCannotBeChanged),

        cmocka_unit_test(Xcp_TestDaqEventSendsDto),
        cmocka_unit_test(Xcp_TestDaqEventSamplesCurrentValues),
        cmocka_unit_test(Xcp_TestDaqEventOnOtherChannelSendsNothing),
        cmocka_unit_test(Xcp_TestDaqStoppedListSendsNothing),
        cmocka_unit_test(Xcp_TestDaqDisconnectedSendsNothing),
        cmocka_unit_test(Xcp_TestDaqPrescaler),
        cmocka_unit_test(Xcp_TestDaqPrescalerZeroIsEveryEvent),
        cmocka_unit_test(Xcp_TestDaqOdtsHaveOwnPid),
        cmocka_unit_test(Xcp_TestDaqEmptyOdtDoesNotBlockLaterOdts),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
