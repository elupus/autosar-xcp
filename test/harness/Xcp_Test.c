#include "Xcp_Test.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include "Det.h"
#include "Os.h"

#if(XCP_PROTOCOL == XCP_PROTOCOL_CAN)
#   include "CanIf.h"
#   include "XcpOnCan_Cbk.h"
#   define XCP_TEST_HEADER 0   /* bytes the transport puts in front of the XCP packet */
#else
#   include "SoAdIf.h"
#   include "XcpOnEth_Cbk.h"
#   define XCP_TEST_HEADER 4   /* length and counter */
#endif

/* ----------------------------- fake lower layer --------------------------- */

static struct {
    uint8_t  data[XCP_TEST_HEADER + XCP_MAX_DTO];
    size_t   len;
    unsigned pdu;
} Xcp_TestFrames[XCP_TEST_MAX_FRAMES];

static size_t   Xcp_TestFrameCount;
static unsigned Xcp_TestFailNext;

static Std_ReturnType Xcp_TestTransmit(PduIdType pduId, const PduInfoType* PduInfoPtr)
{
    if(Xcp_TestFailNext) {
        Xcp_TestFailNext--;
        return E_NOT_OK;
    }
    if(Xcp_TestFrameCount >= XCP_TEST_MAX_FRAMES
    || PduInfoPtr->SduLength > XCP_TEST_HEADER + XCP_MAX_DTO) {
        fail_msg("fake lower layer: too many or too long frames");
    }
    memcpy(Xcp_TestFrames[Xcp_TestFrameCount].data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
    Xcp_TestFrames[Xcp_TestFrameCount].len = PduInfoPtr->SduLength;
    Xcp_TestFrames[Xcp_TestFrameCount].pdu = pduId;
    Xcp_TestFrameCount++;
    return E_OK;
}

#if(XCP_PROTOCOL == XCP_PROTOCOL_CAN)
Std_ReturnType CanIf_Transmit(PduIdType CanTxPduId, const PduInfoType* PduInfoPtr)
{
    return Xcp_TestTransmit(CanTxPduId, PduInfoPtr);
}
#else
Std_ReturnType SoAdIf_Transmit(PduIdType SoAdSrcPduId, const PduInfoType* SoAdSrcPduInfoPtr)
{
    return Xcp_TestTransmit(SoAdSrcPduId, SoAdSrcPduInfoPtr);
}
#endif

size_t         Xcp_TestTxCount(void)          { return Xcp_TestFrameCount; }
size_t         Xcp_TestTxRawLen(size_t idx)   { return Xcp_TestFrames[idx].len; }
const uint8_t* Xcp_TestTxRawData(size_t idx)  { return Xcp_TestFrames[idx].data; }
size_t         Xcp_TestTxLen(size_t idx)      { return Xcp_TestFrames[idx].len - XCP_TEST_HEADER; }
const uint8_t* Xcp_TestTxData(size_t idx)     { return Xcp_TestFrames[idx].data + XCP_TEST_HEADER; }
unsigned       Xcp_TestTxPduId(size_t idx)   { return Xcp_TestFrames[idx].pdu; }
void           Xcp_TestTxFailNext(unsigned n){ Xcp_TestFailNext = n; }

/* ------------------------------- fake Det -------------------------------- */

static unsigned Xcp_TestDetCnt, Xcp_TestDetApi, Xcp_TestDetErr;

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId;
    Xcp_TestDetCnt++;
    Xcp_TestDetApi   = ApiId;
    Xcp_TestDetErr = ErrorId;
    return E_OK;
}

unsigned Xcp_TestDetCount(void)      { return Xcp_TestDetCnt; }
unsigned Xcp_TestDetLastApi(void)   { return Xcp_TestDetApi; }
unsigned Xcp_TestDetLastError(void) { return Xcp_TestDetErr; }

/* -------------------------------- fake Os -------------------------------- */

int GetCounterValue(CounterType id, TickType* value)
{
    (void)id;
    *value = 0;
    return 0;
}

/* -------------------------------- harness -------------------------------- */

static const Xcp_ConfigType Xcp_TestDefaultCfg = {
    .XcpInfo = { .XcpCaption = "XCPTEST" },
};

void Xcp_TestClear(void)
{
    Xcp_TestFrameCount = 0;
}

void Xcp_TestInitWith(const Xcp_ConfigType* cfg)
{
    Xcp_TestFrameCount = 0;
    Xcp_TestFailNext   = 0;
    Xcp_TestDetCnt   = Xcp_TestDetApi = Xcp_TestDetErr = 0;
    Xcp_Init(cfg);
}

void Xcp_TestInit(void)
{
    Xcp_TestInitWith(&Xcp_TestDefaultCfg);
}

static void Xcp_TestRxIndication(PduIdType pduId, PduInfoType* pdu)
{
#if(XCP_PROTOCOL == XCP_PROTOCOL_CAN)
    Xcp_CanIfRxIndication(pduId, pdu);
#else
    Xcp_SoAdIfRxIndication(pduId, pdu);
#endif
}

void Xcp_TestRxRaw(PduIdType pduId, const uint8_t* data, size_t len)
{
    PduInfoType pdu = {
        .SduDataPtr = (uint8*)data,
        .SduLength  = (PduLengthType)len,
    };
    Xcp_TestRxIndication(pduId, &pdu);
}

void Xcp_TestRxNull(PduIdType pduId)
{
    Xcp_TestRxIndication(pduId, NULL);
}

void Xcp_TestSend(const uint8_t* data, size_t len)
{
#if(XCP_PROTOCOL == XCP_PROTOCOL_CAN)
    Xcp_TestRxRaw(XCP_PDU_ID_RX, data, len);
#else
    /* length and counter, little endian, in front of the packet */
    static uint16_t counter;
    uint8_t frame[XCP_TEST_HEADER + XCP_MAX_DTO];
    assert_true(len <= XCP_MAX_DTO);
    frame[0] = len & 0xFF;
    frame[1] = len >> 8;
    frame[2] = counter & 0xFF;
    frame[3] = counter >> 8;
    counter++;
    memcpy(frame + XCP_TEST_HEADER, data, len);
    Xcp_TestRxRaw(XCP_PDU_ID_RX, frame, XCP_TEST_HEADER + len);
#endif
}

void Xcp_TestPump(void)
{
    /* Without XCP_FEATURE_TRANSMIT_FAST one frame is sent per call, and
     * a call may only consume received commands. Bound the loop. */
    for(int i = 0; i < 4 * XCP_TEST_MAX_FRAMES; i++) {
        size_t before = Xcp_TestFrameCount;
        Xcp_MainFunction();
        if(Xcp_TestFrameCount == before && i > 0)
            break;
    }
}

void Xcp_TestConnect(void)
{
    XCP_SEND(XCP_PID_CMD_STD_CONNECT, 0x00);
    Xcp_TestPump();
    assert_true(Xcp_TestTxCount() >= 1);
    assert_int_equal(Xcp_TestTxData(0)[0], XCP_PID_RES);
    Xcp_TestClear();
}

int Xcp_TestRun(const struct CMUnitTest* tests, size_t count)
{
    int failed = 0;
    for(size_t i = 0; i < count; i++) {
        fflush(stdout);
        fflush(stderr);
        pid_t pid = fork();
        if(pid < 0) {
            perror("fork");
            return -1;
        }
        if(pid == 0) {
            int rc = _cmocka_run_group_tests(tests[i].name, &tests[i], 1, NULL, NULL);
            fflush(stdout);
            _exit(rc ? 1 : 0);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("[  FAILED  ] %s (%s)\n", tests[i].name,
                   WIFSIGNALED(status) ? "crashed" : "failed");
            failed++;
        }
    }
    return failed;
}
