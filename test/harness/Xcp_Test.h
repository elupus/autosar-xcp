/* Test harness for the XCP module: fakes the lower layers (CanIf, Det, Os)
 * and gives tests a master-like way of sending commands and checking replies. */
#ifndef XCP_TEST_H_
#define XCP_TEST_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include "Xcp.h"
#include "Xcp_Internal.h"

#define XCP_TEST_MAX_FRAMES 64

/** Memory behind the MTA extension XCP_MTA_EXTENSION_DEBUG (0xFF). SET_MTA
 *  only carries 32 bit addresses so tests address host memory through this. */
extern uint8_t g_XcpDebugMemory[1024];

/**
 * Initialize the module with the default test configuration and clear
 * everything recorded by the fakes.
 */
void Xcp_TestInit(void);

/** Same as Xcp_TestInit() but with a configuration supplied by the test */
void Xcp_TestInitWith(const Xcp_ConfigType* cfg);

/** Send a command as if received from the master (via the CAN transport) */
void Xcp_TestSend(const uint8_t* data, size_t len);

/** Run Xcp_MainFunction() repeatedly until nothing more is transmitted */
void Xcp_TestPump(void);

/** Frames the module has transmitted to the (fake) CanIf */
size_t         Xcp_TestTxCount(void);
size_t         Xcp_TestTxLen(size_t idx);
const uint8_t* Xcp_TestTxData(size_t idx);
unsigned       Xcp_TestTxPduId(size_t idx);

/** Make the next @p n calls to CanIf_Transmit fail with E_NOT_OK */
void Xcp_TestTxFailNext(unsigned n);

/** Errors reported to the (fake) Det */
unsigned Xcp_TestDetCount(void);
unsigned Xcp_TestDetLastApi(void);
unsigned Xcp_TestDetLastError(void);

/**
 * Run each test in a forked process. The module keeps its state in
 * globals and statics that cannot be reset, so this gives every test
 * a pristine module without touching production code.
 * @return number of failed tests
 */
int Xcp_TestRun(const struct CMUnitTest* tests, size_t count);

#define XCP_SEND(...) do {                      \
        const uint8_t XcpBuf_[] = { __VA_ARGS__ }; \
        Xcp_TestSend(XcpBuf_, sizeof(XcpBuf_));       \
    } while(0)

#define XCP_EXPECT_TX(idx, ...) do {                           \
        const uint8_t XcpExp_[] = { __VA_ARGS__ };              \
        assert_true((idx) < Xcp_TestTxCount());                   \
        assert_int_equal(Xcp_TestTxLen(idx), sizeof(XcpExp_));     \
        assert_memory_equal(Xcp_TestTxData(idx), XcpExp_, sizeof(XcpExp_)); \
    } while(0)

/** Send a command, pump the module and check exactly one reply */
#define XCP_EXCHANGE(req, ...) do {  \
        Xcp_TestClear();           \
        XCP_SEND req;                \
        Xcp_TestPump();                  \
        assert_int_equal(Xcp_TestTxCount(), 1); \
        XCP_EXPECT_TX(0, __VA_ARGS__);       \
    } while(0)

void Xcp_TestClear(void);

/** Connect to the slave and drop the connect reply */
void Xcp_TestConnect(void);

#endif
