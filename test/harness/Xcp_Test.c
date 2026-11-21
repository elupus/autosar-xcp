#include "Xcp_Test.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include "CanIf.h"
#include "Det.h"
#include "Os.h"
#include "XcpOnCan_Cbk.h"

/* ------------------------------ fake CanIf ------------------------------ */

static struct {
    uint8_t  data[XCP_MAX_DTO];
    size_t   len;
    unsigned pdu;
} Xcp_TestFrames[XCP_TEST_MAX_FRAMES];

static size_t   Xcp_TestFrameCount;
static unsigned Xcp_TestFailNext;

Std_ReturnType CanIf_Transmit(PduIdType CanTxPduId, const PduInfoType* PduInfoPtr)
{
    if(Xcp_TestFailNext) {
        Xcp_TestFailNext--;
        return E_NOT_OK;
    }
    if(Xcp_TestFrameCount >= XCP_TEST_MAX_FRAMES || PduInfoPtr->SduLength > XCP_MAX_DTO) {
        fail_msg("fake CanIf: too many or too long frames");
    }
    memcpy(Xcp_TestFrames[Xcp_TestFrameCount].data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
    Xcp_TestFrames[Xcp_TestFrameCount].len = PduInfoPtr->SduLength;
    Xcp_TestFrames[Xcp_TestFrameCount].pdu = CanTxPduId;
    Xcp_TestFrameCount++;
    return E_OK;
}

size_t         Xcp_TestTxCount(void)          { return Xcp_TestFrameCount; }
size_t         Xcp_TestTxLen(size_t idx)      { return Xcp_TestFrames[idx].len; }
const uint8_t* Xcp_TestTxData(size_t idx)     { return Xcp_TestFrames[idx].data; }
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

void Xcp_TestSend(const uint8_t* data, size_t len)
{
    PduInfoType pdu = {
        .SduDataPtr = (uint8*)data,
        .SduLength  = (PduLengthType)len,
    };
    Xcp_CanIfRxIndication(XCP_PDU_ID_RX, &pdu);
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
