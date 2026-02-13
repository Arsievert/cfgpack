/* Direct unit tests for the MessagePack encode/decode layer. */

#include "cfgpack/msgpack.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: encode then decode a uint64
 * ═══════════════════════════════════════════════════════════════════════════ */
static test_result_t roundtrip_uint64(uint64_t val) {
    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    CHECK(cfgpack_msgpack_encode_uint64(&buf, val) == CFGPACK_OK);

    cfgpack_reader_t r;
    cfgpack_reader_init(&r, storage, buf.len);
    uint64_t out = 0;
    CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_OK);
    CHECK(out == val);
    CHECK(r.pos == buf.len); /* fully consumed */
    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: encode then decode an int64
 * ═══════════════════════════════════════════════════════════════════════════ */
static test_result_t roundtrip_int64(int64_t val) {
    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    CHECK(cfgpack_msgpack_encode_int64(&buf, val) == CFGPACK_OK);

    cfgpack_reader_t r;
    cfgpack_reader_init(&r, storage, buf.len);
    int64_t out = 0;
    CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_OK);
    CHECK(out == val);
    CHECK(r.pos == buf.len);
    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. Positive fixint range (0..127)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_uint_fixint) {
    LOG_SECTION("uint64 roundtrip: positive fixint range");

    LOG("Testing value 0");
    CHECK(roundtrip_uint64(0) == TEST_OK);
    LOG("Testing value 1");
    CHECK(roundtrip_uint64(1) == TEST_OK);
    LOG("Testing value 127");
    CHECK(roundtrip_uint64(127) == TEST_OK);

    /* Verify encoding length: single byte for fixint */
    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_uint64(&buf, 127);
    CHECK(buf.len == 1);
    LOG("Encoding length for 127 = %zu byte (fixint)", buf.len);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. uint8 range (128..255)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_uint8) {
    LOG_SECTION("uint64 roundtrip: uint8 format (0xcc)");

    LOG("Testing value 128");
    CHECK(roundtrip_uint64(128) == TEST_OK);
    LOG("Testing value 255");
    CHECK(roundtrip_uint64(255) == TEST_OK);

    /* Verify 0xcc prefix */
    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_uint64(&buf, 128);
    CHECK(buf.len == 2);
    CHECK(storage[0] == 0xcc);
    LOG("Encoding: 0x%02x 0x%02x (0xcc format, 2 bytes)", storage[0],
        storage[1]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. uint16 range (256..65535)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_uint16) {
    LOG_SECTION("uint64 roundtrip: uint16 format (0xcd)");

    LOG("Testing value 256");
    CHECK(roundtrip_uint64(256) == TEST_OK);
    LOG("Testing value 65535");
    CHECK(roundtrip_uint64(65535) == TEST_OK);

    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_uint64(&buf, 256);
    CHECK(buf.len == 3);
    CHECK(storage[0] == 0xcd);
    LOG("Encoding: 0x%02x (0xcd format, 3 bytes)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. uint32 range (65536..0xFFFFFFFF)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_uint32) {
    LOG_SECTION("uint64 roundtrip: uint32 format (0xce)");

    LOG("Testing value 65536");
    CHECK(roundtrip_uint64(65536) == TEST_OK);
    LOG("Testing value 0xFFFFFFFF");
    CHECK(roundtrip_uint64(0xFFFFFFFFu) == TEST_OK);

    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_uint64(&buf, 65536);
    CHECK(buf.len == 5);
    CHECK(storage[0] == 0xce);
    LOG("Encoding: 0x%02x (0xce format, 5 bytes)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. uint64 range (>32 bits)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_uint64) {
    LOG_SECTION("uint64 roundtrip: uint64 format (0xcf)");

    uint64_t big = (uint64_t)0x100000000ULL;
    LOG("Testing value 0x100000000");
    CHECK(roundtrip_uint64(big) == TEST_OK);

    uint64_t max = UINT64_MAX;
    LOG("Testing value UINT64_MAX");
    CHECK(roundtrip_uint64(max) == TEST_OK);

    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_uint64(&buf, big);
    CHECK(buf.len == 9);
    CHECK(storage[0] == 0xcf);
    LOG("Encoding: 0x%02x (0xcf format, 9 bytes)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. Negative fixint range (-1..-32)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_int_fixint) {
    LOG_SECTION("int64 roundtrip: negative fixint range");

    LOG("Testing value -1");
    CHECK(roundtrip_int64(-1) == TEST_OK);
    LOG("Testing value -32");
    CHECK(roundtrip_int64(-32) == TEST_OK);

    /* Verify single-byte encoding */
    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_int64(&buf, -1);
    CHECK(buf.len == 1);
    CHECK(storage[0] == 0xff); /* -1 as negative fixint */
    LOG("Encoding of -1: 0x%02x (1 byte, negative fixint)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. int8 range (-33..-128)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_int8) {
    LOG_SECTION("int64 roundtrip: int8 format (0xd0)");

    LOG("Testing value -33");
    CHECK(roundtrip_int64(-33) == TEST_OK);
    LOG("Testing value -128");
    CHECK(roundtrip_int64(-128) == TEST_OK);

    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_int64(&buf, -33);
    CHECK(buf.len == 2);
    CHECK(storage[0] == 0xd0);
    LOG("Encoding: 0x%02x (0xd0 format, 2 bytes)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. int16 range (-129..-32768)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_int16) {
    LOG_SECTION("int64 roundtrip: int16 format (0xd1)");

    LOG("Testing value -129");
    CHECK(roundtrip_int64(-129) == TEST_OK);
    LOG("Testing value -32768");
    CHECK(roundtrip_int64(-32768) == TEST_OK);

    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_int64(&buf, -129);
    CHECK(buf.len == 3);
    CHECK(storage[0] == 0xd1);
    LOG("Encoding: 0x%02x (0xd1 format, 3 bytes)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. int32 range (-32769..INT32_MIN)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_int32) {
    LOG_SECTION("int64 roundtrip: int32 format (0xd2)");

    LOG("Testing value -32769");
    CHECK(roundtrip_int64(-32769) == TEST_OK);
    LOG("Testing value INT32_MIN (-2147483648)");
    CHECK(roundtrip_int64(INT32_MIN) == TEST_OK);

    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_int64(&buf, -32769);
    CHECK(buf.len == 5);
    CHECK(storage[0] == 0xd2);
    LOG("Encoding: 0x%02x (0xd2 format, 5 bytes)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. int64 range (< INT32_MIN)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_int64) {
    LOG_SECTION("int64 roundtrip: int64 format (0xd3)");

    int64_t val = (int64_t)INT32_MIN - 1;
    LOG("Testing value INT32_MIN - 1 = %lld", (long long)val);
    CHECK(roundtrip_int64(val) == TEST_OK);

    LOG("Testing value INT64_MIN");
    CHECK(roundtrip_int64(INT64_MIN) == TEST_OK);

    uint8_t storage[16];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_int64(&buf, val);
    CHECK(buf.len == 9);
    CHECK(storage[0] == 0xd3);
    LOG("Encoding: 0x%02x (0xd3 format, 9 bytes)", storage[0]);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. float32 roundtrip
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_f32) {
    LOG_SECTION("float32 roundtrip");

    float test_vals[] = {0.0f, -1.5f, 3.14159f};
    for (size_t i = 0; i < sizeof(test_vals) / sizeof(test_vals[0]); ++i) {
        float val = test_vals[i];
        uint8_t storage[16];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_f32(&buf, val) == CFGPACK_OK);
        CHECK(buf.len == 5);
        CHECK(storage[0] == 0xca);

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        float out = 0;
        CHECK(cfgpack_msgpack_decode_f32(&r, &out) == CFGPACK_OK);
        CHECK(out == val);
        LOG("f32 roundtrip: %f -> %f (ok)", (double)val, (double)out);
    }

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 12. float64 roundtrip
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_f64) {
    LOG_SECTION("float64 roundtrip");

    double test_vals[] = {0.0, -1.5e100, 2.718281828};
    for (size_t i = 0; i < sizeof(test_vals) / sizeof(test_vals[0]); ++i) {
        double val = test_vals[i];
        uint8_t storage[16];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_f64(&buf, val) == CFGPACK_OK);
        CHECK(buf.len == 9);
        CHECK(storage[0] == 0xcb);

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        double out = 0;
        CHECK(cfgpack_msgpack_decode_f64(&r, &out) == CFGPACK_OK);
        CHECK(out == val);
        LOG("f64 roundtrip: %g -> %g (ok)", val, out);
    }

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 13. String roundtrip (fixstr, str8, str16)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_str) {
    LOG_SECTION("String roundtrip: fixstr, str8, str16");

    /* Empty string (fixstr with len 0) */
    {
        uint8_t storage[16];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_str(&buf, "", 0) == CFGPACK_OK);
        CHECK(buf.len == 1);
        CHECK(storage[0] == 0xa0);

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        const uint8_t *ptr;
        uint32_t len;
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_OK);
        CHECK(len == 0);
        LOG("Empty string: len=0 (ok)");
    }

    /* Short fixstr (5 bytes) */
    {
        const char *s = "hello";
        uint8_t storage[32];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_str(&buf, s, 5) == CFGPACK_OK);
        CHECK(storage[0] == (0xa0 | 5));
        LOG("Fixstr 'hello': header=0x%02x (ok)", storage[0]);

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        const uint8_t *ptr;
        uint32_t len;
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_OK);
        CHECK(len == 5);
        CHECK(memcmp(ptr, "hello", 5) == 0);
        LOG("Decoded: len=%u, content matches (ok)", len);
    }

    /* str8 (32 bytes, exceeds fixstr max of 31) */
    {
        char s32[33];
        memset(s32, 'A', 32);
        s32[32] = '\0';
        uint8_t storage[64];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_str(&buf, s32, 32) == CFGPACK_OK);
        CHECK(storage[0] == 0xd9);
        CHECK(storage[1] == 32);
        LOG("str8 (32 bytes): header=0xd9 0x20 (ok)");

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        const uint8_t *ptr;
        uint32_t len;
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_OK);
        CHECK(len == 32);
        CHECK(memcmp(ptr, s32, 32) == 0);
        LOG("Decoded: len=%u (ok)", len);
    }

    /* str16 (256 bytes) */
    {
        char s256[257];
        memset(s256, 'B', 256);
        s256[256] = '\0';
        uint8_t storage[512];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_str(&buf, s256, 256) == CFGPACK_OK);
        CHECK(storage[0] == 0xda);
        CHECK(storage[1] == 0x01);
        CHECK(storage[2] == 0x00);
        LOG("str16 (256 bytes): header=0xda 0x01 0x00 (ok)");

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        const uint8_t *ptr;
        uint32_t len;
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_OK);
        CHECK(len == 256);
        CHECK(memcmp(ptr, s256, 256) == 0);
        LOG("Decoded: len=%u (ok)", len);
    }

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 14. Map header roundtrip (fixmap, map16)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_decode_map_header) {
    LOG_SECTION("Map header roundtrip: fixmap and map16");

    /* fixmap: count 0 */
    {
        uint8_t storage[8];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_map_header(&buf, 0) == CFGPACK_OK);
        CHECK(buf.len == 1);
        CHECK(storage[0] == 0x80);

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        uint32_t count = 99;
        CHECK(cfgpack_msgpack_decode_map_header(&r, &count) == CFGPACK_OK);
        CHECK(count == 0);
        LOG("fixmap(0): ok");
    }

    /* fixmap: count 15 (max for fixmap) */
    {
        uint8_t storage[8];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_map_header(&buf, 15) == CFGPACK_OK);
        CHECK(buf.len == 1);
        CHECK(storage[0] == (0x80 | 15));

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        uint32_t count = 0;
        CHECK(cfgpack_msgpack_decode_map_header(&r, &count) == CFGPACK_OK);
        CHECK(count == 15);
        LOG("fixmap(15): ok");
    }

    /* map16: count 16 */
    {
        uint8_t storage[8];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_map_header(&buf, 16) == CFGPACK_OK);
        CHECK(buf.len == 3);
        CHECK(storage[0] == 0xde);

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        uint32_t count = 0;
        CHECK(cfgpack_msgpack_decode_map_header(&r, &count) == CFGPACK_OK);
        CHECK(count == 16);
        LOG("map16(16): ok");
    }

    /* map16: count 1000 */
    {
        uint8_t storage[8];
        cfgpack_buf_t buf;
        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_map_header(&buf, 1000) == CFGPACK_OK);
        CHECK(buf.len == 3);

        cfgpack_reader_t r;
        cfgpack_reader_init(&r, storage, buf.len);
        uint32_t count = 0;
        CHECK(cfgpack_msgpack_decode_map_header(&r, &count) == CFGPACK_OK);
        CHECK(count == 1000);
        LOG("map16(1000): ok");
    }

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 15. skip_value for all types
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_skip_value_all_types) {
    LOG_SECTION("skip_value correctly advances past every msgpack type");

    uint8_t storage[256];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));

    /* Encode a series of different types sequentially */
    CHECK(cfgpack_msgpack_encode_uint64(&buf, 42) == CFGPACK_OK);       /* fixint */
    CHECK(cfgpack_msgpack_encode_uint64(&buf, 200) == CFGPACK_OK);      /* u8 */
    CHECK(cfgpack_msgpack_encode_uint64(&buf, 1000) == CFGPACK_OK);     /* u16 */
    CHECK(cfgpack_msgpack_encode_uint64(&buf, 100000) == CFGPACK_OK);   /* u32 */
    CHECK(cfgpack_msgpack_encode_uint64(&buf, 0x100000000ULL) ==
          CFGPACK_OK);                                                   /* u64 */
    CHECK(cfgpack_msgpack_encode_int64(&buf, -1) == CFGPACK_OK);        /* neg fixint */
    CHECK(cfgpack_msgpack_encode_int64(&buf, -100) == CFGPACK_OK);      /* i8 */
    CHECK(cfgpack_msgpack_encode_int64(&buf, -1000) == CFGPACK_OK);     /* i16 */
    CHECK(cfgpack_msgpack_encode_int64(&buf, -100000) == CFGPACK_OK);   /* i32 */
    CHECK(cfgpack_msgpack_encode_int64(&buf, (int64_t)INT32_MIN - 1) ==
          CFGPACK_OK);                                                   /* i64 */
    CHECK(cfgpack_msgpack_encode_f32(&buf, 1.0f) == CFGPACK_OK);        /* f32 */
    CHECK(cfgpack_msgpack_encode_f64(&buf, 2.0) == CFGPACK_OK);         /* f64 */
    CHECK(cfgpack_msgpack_encode_str(&buf, "hi", 2) == CFGPACK_OK);     /* fixstr */
    CHECK(cfgpack_msgpack_encode_map_header(&buf, 0) == CFGPACK_OK);    /* empty fixmap */

    /* Also add nil (0xc0), false (0xc2), true (0xc3) manually */
    uint8_t nil = 0xc0;
    CHECK(cfgpack_buf_append(&buf, &nil, 1) == CFGPACK_OK);
    uint8_t false_byte = 0xc2;
    CHECK(cfgpack_buf_append(&buf, &false_byte, 1) == CFGPACK_OK);
    uint8_t true_byte = 0xc3;
    CHECK(cfgpack_buf_append(&buf, &true_byte, 1) == CFGPACK_OK);

    size_t total_len = buf.len;
    LOG("Encoded %zu bytes of mixed types", total_len);

    /* Now skip all values and verify we consumed the entire buffer */
    cfgpack_reader_t r;
    cfgpack_reader_init(&r, storage, total_len);

    int count = 0;
    while (r.pos < r.len) {
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        count++;
    }
    CHECK(r.pos == total_len);
    LOG("Skipped %d values, consumed all %zu bytes", count, total_len);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 16. buf_append overflow
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_buf_append_overflow) {
    LOG_SECTION("buf_append returns ERR_ENCODE when buffer is full");

    uint8_t storage[4];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, storage, sizeof(storage));

    LOG("Buffer capacity: %zu bytes", buf.cap);

    /* Fill buffer completely */
    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    CHECK(cfgpack_buf_append(&buf, data, 4) == CFGPACK_OK);
    LOG("Appended 4 bytes (buffer full)");

    /* Try to append one more byte */
    uint8_t extra = 0x05;
    CHECK(cfgpack_buf_append(&buf, &extra, 1) == CFGPACK_ERR_ENCODE);
    LOG("Extra byte rejected: CFGPACK_ERR_ENCODE (ok)");

    /* Also test encode_uint64 with tiny buffer */
    uint8_t tiny[1];
    cfgpack_buf_t tiny_buf;
    cfgpack_buf_init(&tiny_buf, tiny, sizeof(tiny));
    CHECK(cfgpack_msgpack_encode_uint64(&tiny_buf, 256) ==
          CFGPACK_ERR_ENCODE); /* needs 3 bytes */
    LOG("encode_uint64(256) in 1-byte buffer rejected (ok)");

    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |=
        (test_case_result("encode_decode_uint_fixint",
                          test_encode_decode_uint_fixint()) != TEST_OK);
    overall |= (test_case_result("encode_decode_uint8",
                                 test_encode_decode_uint8()) != TEST_OK);
    overall |= (test_case_result("encode_decode_uint16",
                                 test_encode_decode_uint16()) != TEST_OK);
    overall |= (test_case_result("encode_decode_uint32",
                                 test_encode_decode_uint32()) != TEST_OK);
    overall |= (test_case_result("encode_decode_uint64",
                                 test_encode_decode_uint64()) != TEST_OK);
    overall |=
        (test_case_result("encode_decode_int_fixint",
                          test_encode_decode_int_fixint()) != TEST_OK);
    overall |= (test_case_result("encode_decode_int8",
                                 test_encode_decode_int8()) != TEST_OK);
    overall |= (test_case_result("encode_decode_int16",
                                 test_encode_decode_int16()) != TEST_OK);
    overall |= (test_case_result("encode_decode_int32",
                                 test_encode_decode_int32()) != TEST_OK);
    overall |= (test_case_result("encode_decode_int64",
                                 test_encode_decode_int64()) != TEST_OK);
    overall |=
        (test_case_result("encode_decode_f32", test_encode_decode_f32()) !=
         TEST_OK);
    overall |=
        (test_case_result("encode_decode_f64", test_encode_decode_f64()) !=
         TEST_OK);
    overall |=
        (test_case_result("encode_decode_str", test_encode_decode_str()) !=
         TEST_OK);
    overall |= (test_case_result("encode_decode_map_header",
                                 test_encode_decode_map_header()) != TEST_OK);
    overall |= (test_case_result("skip_value_all_types",
                                 test_skip_value_all_types()) != TEST_OK);
    overall |= (test_case_result("buf_append_overflow",
                                 test_buf_append_overflow()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
