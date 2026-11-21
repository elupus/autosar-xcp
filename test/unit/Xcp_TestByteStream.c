#include "Xcp_Test.h"

#include <string.h>

/* Bytes 0x01 .. 0x0F, so a value read at any offset is easy to tell apart */
static void Xcp_TestByteStreamFill(uint8_t* buf)
{
    for(int i = 0; i < 16; i++)
        buf[i] = (uint8_t)(i + 1);
}

/* The accessors use the byte order of the host, which is what Connect reports */
static void Xcp_TestByteStreamGetUint16(void** state)
{
    (void)state;
    uint8_t buf[16];
    Xcp_TestByteStreamFill(buf);

    for(int off = 0; off < 8; off++) {
        uint16_t expect;
        memcpy(&expect, buf + off, sizeof(expect));
        assert_int_equal(GET_UINT16(buf, off), expect);
    }
}

static void Xcp_TestByteStreamGetUint32(void** state)
{
    (void)state;
    uint8_t buf[16];
    Xcp_TestByteStreamFill(buf);

    for(int off = 0; off < 8; off++) {
        uint32_t expect;
        memcpy(&expect, buf + off, sizeof(expect));
        assert_int_equal(GET_UINT32(buf, off), expect);
    }
}

static void Xcp_TestByteStreamSetUint16(void** state)
{
    (void)state;
    for(int off = 0; off < 8; off++) {
        uint8_t buf[16];
        Xcp_TestByteStreamFill(buf);

        SET_UINT16(buf, off, 0xA1B2);

        uint8_t expect[16];
        Xcp_TestByteStreamFill(expect);
        uint16_t value = 0xA1B2;
        memcpy(expect + off, &value, sizeof(value));
        assert_memory_equal(buf, expect, sizeof(buf));
    }
}

static void Xcp_TestByteStreamSetUint32(void** state)
{
    (void)state;
    for(int off = 0; off < 8; off++) {
        uint8_t buf[16];
        Xcp_TestByteStreamFill(buf);

        SET_UINT32(buf, off, 0xA1B2C3D4u);

        uint8_t expect[16];
        Xcp_TestByteStreamFill(expect);
        uint32_t value = 0xA1B2C3D4u;
        memcpy(expect + off, &value, sizeof(value));
        assert_memory_equal(buf, expect, sizeof(buf));
    }
}

static void Xcp_TestByteStreamRoundTrip(void** state)
{
    (void)state;
    uint8_t buf[16] = { 0 };

    SET_UINT16(buf, 1, 0x1234);
    SET_UINT32(buf, 5, 0x89ABCDEFu);
    assert_int_equal(GET_UINT16(buf, 1), 0x1234);
    assert_int_equal(GET_UINT32(buf, 5), 0x89ABCDEFu);
    assert_int_equal(GET_UINT8(buf, 0), 0);
}

static void Xcp_TestByteStreamSetTruncatesToWidth(void** state)
{
    (void)state;
    uint8_t buf[8];
    memset(buf, 0xEE, sizeof(buf));

    SET_UINT16(buf, 1, 0x12345);
    assert_int_equal(GET_UINT16(buf, 1), 0x2345);
    assert_int_equal(buf[0], 0xEE);
    assert_int_equal(buf[3], 0xEE);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(Xcp_TestByteStreamGetUint16),
        cmocka_unit_test(Xcp_TestByteStreamGetUint32),
        cmocka_unit_test(Xcp_TestByteStreamSetUint16),
        cmocka_unit_test(Xcp_TestByteStreamSetUint32),
        cmocka_unit_test(Xcp_TestByteStreamRoundTrip),
        cmocka_unit_test(Xcp_TestByteStreamSetTruncatesToWidth),
    };
    return Xcp_TestRun(tests, sizeof(tests) / sizeof(tests[0])) ? 1 : 0;
}
