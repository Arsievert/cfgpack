/* Tests for msgpack decoder paths unreachable via encoder roundtrips. */

#include "cfgpack/msgpack.h"

#include "test.h"

#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. decode_uint64 — truncation and invalid format
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_uint64_errors) {
    cfgpack_reader_t r;
    uint64_t out;

    LOG_SECTION("decode_uint64 truncation and invalid format");

    /* Empty buffer */
    {
        uint8_t data[] = {0};
        cfgpack_reader_init(&r, data, 0);
        CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Empty buffer: ERR_DECODE (ok)");
    }

    /* uint8 (0xcc) with no payload */
    {
        uint8_t data[] = {0xcc};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xcc truncated: ERR_DECODE (ok)");
    }

    /* uint16 (0xcd) with 1 payload byte (needs 2) */
    {
        uint8_t data[] = {0xcd, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xcd truncated: ERR_DECODE (ok)");
    }

    /* uint32 (0xce) with 3 payload bytes (needs 4) */
    {
        uint8_t data[] = {0xce, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xce truncated: ERR_DECODE (ok)");
    }

    /* uint64 (0xcf) with 7 payload bytes (needs 8) */
    {
        uint8_t data[] = {0xcf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xcf truncated: ERR_DECODE (ok)");
    }

    /* Invalid format byte (0xca = float32) */
    {
        uint8_t data[] = {0xca};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Invalid format 0xca: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. decode_int64 — positive fixint and error paths
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_int64_positive_and_errors) {
    cfgpack_reader_t r;
    int64_t out;

    LOG_SECTION("decode_int64 positive fixint (uncovered by roundtrips)");

    /* Positive fixint: 0x00 -> 0 */
    {
        uint8_t data[] = {0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        out = -1;
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_OK);
        CHECK(out == 0);
        LOG("Positive fixint 0x00 -> %lld (ok)", (long long)out);
    }

    /* Positive fixint: 0x7f -> 127 */
    {
        uint8_t data[] = {0x7f};
        cfgpack_reader_init(&r, data, sizeof(data));
        out = -1;
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_OK);
        CHECK(out == 127);
        LOG("Positive fixint 0x7f -> %lld (ok)", (long long)out);
    }

    /* Positive fixint: 0x2a -> 42 */
    {
        uint8_t data[] = {0x2a};
        cfgpack_reader_init(&r, data, sizeof(data));
        out = -1;
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_OK);
        CHECK(out == 42);
        LOG("Positive fixint 0x2a -> %lld (ok)", (long long)out);
    }

    LOG_SECTION("decode_int64 truncation errors");

    /* Empty buffer */
    {
        uint8_t data[] = {0};
        cfgpack_reader_init(&r, data, 0);
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Empty buffer: ERR_DECODE (ok)");
    }

    /* int8 (0xd0) with no payload */
    {
        uint8_t data[] = {0xd0};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xd0 truncated: ERR_DECODE (ok)");
    }

    /* int16 (0xd1) with 1 payload byte (needs 2) */
    {
        uint8_t data[] = {0xd1, 0xff};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xd1 truncated: ERR_DECODE (ok)");
    }

    /* int32 (0xd2) with 3 payload bytes (needs 4) */
    {
        uint8_t data[] = {0xd2, 0xff, 0xff, 0xff};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xd2 truncated: ERR_DECODE (ok)");
    }

    /* int64 (0xd3) with 7 payload bytes (needs 8) */
    {
        uint8_t data[] = {0xd3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("0xd3 truncated: ERR_DECODE (ok)");
    }

    LOG_SECTION("decode_int64 invalid format");

    /* 0xcc (uint8) is not handled by int64 decoder */
    {
        uint8_t data[] = {0xcc, 0x05};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Invalid format 0xcc: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2b. decode_int64 — full-width 0xd3 round-trips and high-bit payloads
 *
 * Regression for the signed-left-shift UB in the 0xd3 assembly loop: values
 * with the sign bit set used to shift into the sign region of an int64_t.
 * Assembly now happens in uint64_t, so these must decode cleanly and exactly.
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_int64_full_width_roundtrip) {
    cfgpack_reader_t r;
    int64_t out;

    LOG_SECTION("decode_int64 full-width (0xd3) values");

    /* A 0xd3 tag is always 8 big-endian payload bytes. Drive the assembly loop
     * directly across the full range — including sign-bit-set values that used
     * to overflow the signed shift — and check exact two's-complement results.
     * (Positive values are encoded as unsigned formats by the encoder, so a
     * true encode/decode round-trip can't reach the 0xd3 branch; building the
     * payload by hand is what targets the fixed loop.) */
    {
        static const int64_t cases[] = {
            INT64_MIN,
            INT64_MAX,
            INT64_MIN + 1,
            INT64_MAX - 1,
            -1,
            (int64_t)INT32_MIN - 1,
            -4000000000LL,
            0x0123456789abcdefLL,
            (int64_t)0xfedcba9876543210ULL,
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            uint64_t bits = (uint64_t)cases[i];
            uint8_t data[9];
            int j;

            data[0] = 0xd3;
            for (j = 0; j < 8; ++j) {
                data[1 + j] = (uint8_t)(bits >> (8 * (7 - j)));
            }
            cfgpack_reader_init(&r, data, sizeof(data));
            out = 0;
            CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_OK);
            CHECK(out == cases[i]);
            LOG("0xd3 %lld -> %lld (ok)", (long long)cases[i], (long long)out);
        }
    }

    LOG_SECTION("decode_int64 raw high-bit 0xd3 payload (issue repro)");

    /* The exact malformed-but-well-formed payload from the bug report. Decoding
     * must be deterministic (no UB) and equal the two's-complement value. */
    {
        uint8_t data[] = {0xd3, 0xd6, 0xcb, 0xa2, 0xfb, 0x7d, 0x0a, 0x39, 0x27};
        cfgpack_reader_init(&r, data, sizeof(data));
        out = 0;
        CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_OK);
        CHECK(out == (int64_t)0xd6cba2fb7d0a3927ULL);
        LOG("0xd3 high-bit payload -> %lld (ok)", (long long)out);
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2c. decode_int64 — int16 (0xd1) byte-order regression
 *
 * The 0xd1 path used to memcpy the two big-endian wire bytes into a native
 * int16_t and byte-swap unconditionally, which is only correct on
 * little-endian hosts.  Assembly now composes from uint8_t bytes like every
 * other integer decoder, so these exact values must decode identically on
 * any host endianness (exercised by the big-endian CI leg).
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_int16_byte_order) {
    cfgpack_reader_t r;
    int64_t out;

    LOG_SECTION("decode_int64 int16 (0xd1) exact values");

    {
        static const struct {
            uint8_t hi;
            uint8_t lo;
            int64_t expect;
        } cases[] = {
            {0x12, 0x34, 0x1234}, {0xff, 0xfe, -2},   {0x80, 0x00, -32768},
            {0x7f, 0xff, 32767},  {0xff, 0x7f, -129}, {0x00, 0x80, 128},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            uint8_t data[] = {0xd1, cases[i].hi, cases[i].lo};
            cfgpack_reader_init(&r, data, sizeof(data));
            out = 0;
            CHECK(cfgpack_msgpack_decode_int64(&r, &out) == CFGPACK_OK);
            CHECK(out == cases[i].expect);
            LOG("0xd1 %02x %02x -> %lld (ok)", cases[i].hi, cases[i].lo,
                (long long)out);
        }
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2d. skip_value — 32-bit length-field bounds
 *
 * str32/bin32 lengths are attacker-controlled up to 0xFFFFFFFF.  The bounds
 * check used to be written as pos + len > total, which wraps on 32-bit
 * size_t and lets the cursor move backwards.  The check is now written by
 * subtraction; a huge declared length must be rejected on every platform
 * (exercised by the 32-bit CI leg).
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_skip_len32_overflow) {
    cfgpack_reader_t r;

    LOG_SECTION("skip_value str32/bin32 with near-UINT32_MAX lengths");

    /* str 32 declaring 0xFFFFFFFF bytes in a 6-byte buffer */
    {
        uint8_t data[] = {0xdb, 0xff, 0xff, 0xff, 0xff, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("str32 len=0xffffffff: ERR_DECODE (ok)");
    }

    /* bin 32 declaring 0xFFFFFFFB bytes: on 32-bit size_t, pos(5) + len
     * used to wrap to 0 and pass the old additive check */
    {
        uint8_t data[] = {0xc6, 0xff, 0xff, 0xff, 0xfb, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("bin32 len=0xfffffffb: ERR_DECODE (ok)");
    }

    /* str 32 nested one level down: same rejection inside a container */
    {
        uint8_t data[] = {0x91, 0xdb, 0xff, 0xff, 0xff, 0xfa};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("nested str32 huge len: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2e. encoders — length caps
 *
 * encode_str and encode_map_header emit at most 16-bit length headers; a
 * larger length used to be silently truncated into the header while all the
 * payload bytes were appended (corrupt output).  They must now refuse.
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_length_caps) {
    uint8_t storage[16];
    cfgpack_buf_t buf;
    char dummy = 'x';

    LOG_SECTION("encode_str / encode_map_header reject > 16-bit lengths");

    /* Length is validated before the source pointer is read, so a 1-byte
     * source with an oversized length is safe here. */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    CHECK(cfgpack_msgpack_encode_str(&buf, &dummy, 0x10000) ==
          CFGPACK_ERR_ENCODE);
    CHECK(buf.len == 0);
    LOG("encode_str len=0x10000: ERR_ENCODE, nothing written (ok)");

    cfgpack_buf_init(&buf, storage, sizeof(storage));
    CHECK(cfgpack_msgpack_encode_map_header(&buf, 0x10000) ==
          CFGPACK_ERR_ENCODE);
    CHECK(buf.len == 0);
    LOG("encode_map_header count=0x10000: ERR_ENCODE, nothing written (ok)");

    /* Boundary: exactly 0xffff is still legal for a map header */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    CHECK(cfgpack_msgpack_encode_map_header(&buf, 0xffff) == CFGPACK_OK);
    CHECK(buf.len == 3 && storage[0] == 0xde && storage[1] == 0xff &&
          storage[2] == 0xff);
    LOG("encode_map_header count=0xffff: map16 header (ok)");

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. decode_f32 — error paths
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_f32_errors) {
    cfgpack_reader_t r;
    float out;

    LOG_SECTION("decode_f32 error paths");

    /* Empty buffer */
    {
        uint8_t data[] = {0};
        cfgpack_reader_init(&r, data, 0);
        CHECK(cfgpack_msgpack_decode_f32(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Empty buffer: ERR_DECODE (ok)");
    }

    /* Wrong type byte (0xcb = f64) */
    {
        uint8_t data[] = {0xcb, 0x00, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_f32(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Wrong type 0xcb: ERR_DECODE (ok)");
    }

    /* Truncated payload (0xca + only 3 data bytes, needs 4) */
    {
        uint8_t data[] = {0xca, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_f32(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Truncated payload: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. decode_f64 — error paths
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_f64_errors) {
    cfgpack_reader_t r;
    double out;

    LOG_SECTION("decode_f64 error paths");

    /* Empty buffer */
    {
        uint8_t data[] = {0};
        cfgpack_reader_init(&r, data, 0);
        CHECK(cfgpack_msgpack_decode_f64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Empty buffer: ERR_DECODE (ok)");
    }

    /* Wrong type byte (0xca = f32) */
    {
        uint8_t data[] = {0xca, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_f64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Wrong type 0xca: ERR_DECODE (ok)");
    }

    /* Truncated payload (0xcb + only 7 data bytes, needs 8) */
    {
        uint8_t data[] = {0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_f64(&r, &out) == CFGPACK_ERR_DECODE);
        LOG("Truncated payload: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. decode_str — error paths
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_str_errors) {
    cfgpack_reader_t r;
    const uint8_t *ptr;
    uint32_t len;

    LOG_SECTION("decode_str error paths");

    /* Empty buffer */
    {
        uint8_t data[] = {0};
        cfgpack_reader_init(&r, data, 0);
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_ERR_DECODE);
        LOG("Empty buffer: ERR_DECODE (ok)");
    }

    /* fixstr body truncated: header says 5 bytes, only 3 available */
    {
        uint8_t data[] = {0xa5, 'a', 'b', 'c'};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_ERR_DECODE);
        LOG("fixstr body truncated: ERR_DECODE (ok)");
    }

    /* str8 (0xd9) with no length byte */
    {
        uint8_t data[] = {0xd9};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_ERR_DECODE);
        LOG("str8 no length: ERR_DECODE (ok)");
    }

    /* str8 (0xd9) with length but truncated body */
    {
        uint8_t data[] = {0xd9, 0x05, 'a', 'b'};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_ERR_DECODE);
        LOG("str8 body truncated: ERR_DECODE (ok)");
    }

    /* str16 (0xda) with only 1 length byte (needs 2) */
    {
        uint8_t data[] = {0xda, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_ERR_DECODE);
        LOG("str16 length truncated: ERR_DECODE (ok)");
    }

    /* str16 (0xda) with length but truncated body */
    {
        uint8_t data[] = {0xda, 0x00, 0x05, 'a', 'b'};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_ERR_DECODE);
        LOG("str16 body truncated: ERR_DECODE (ok)");
    }

    /* Invalid type byte (0xcc = uint8) */
    {
        uint8_t data[] = {0xcc, 0x05};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_ERR_DECODE);
        LOG("Invalid type 0xcc: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. decode_map_header — error paths
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_decode_map_header_errors) {
    cfgpack_reader_t r;
    uint32_t count;

    LOG_SECTION("decode_map_header error paths");

    /* Empty buffer */
    {
        uint8_t data[] = {0};
        cfgpack_reader_init(&r, data, 0);
        CHECK(cfgpack_msgpack_decode_map_header(&r, &count) ==
              CFGPACK_ERR_DECODE);
        LOG("Empty buffer: ERR_DECODE (ok)");
    }

    /* map16 (0xde) with only 1 length byte (needs 2) */
    {
        uint8_t data[] = {0xde, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_map_header(&r, &count) ==
              CFGPACK_ERR_DECODE);
        LOG("map16 truncated: ERR_DECODE (ok)");
    }

    /* Invalid type byte (0xcc = uint8) */
    {
        uint8_t data[] = {0xcc};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_decode_map_header(&r, &count) ==
              CFGPACK_ERR_DECODE);
        LOG("Invalid type 0xcc: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. skip_value — container types
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_skip_containers) {
    cfgpack_reader_t r;

    LOG_SECTION("skip_value with non-empty containers");

    /* Non-empty fixmap: {1: 2} */
    {
        uint8_t data[] = {0x81, 0x01, 0x02};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("fixmap {1:2}: skipped %zu bytes (ok)", r.pos);
    }

    /* Empty fixarray */
    {
        uint8_t data[] = {0x90};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == 1);
        LOG("Empty fixarray: skipped (ok)");
    }

    /* Non-empty fixarray: [1, 2] */
    {
        uint8_t data[] = {0x92, 0x01, 0x02};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("fixarray [1,2]: skipped %zu bytes (ok)", r.pos);
    }

    /* Nested: [[1]] */
    {
        uint8_t data[] = {0x91, 0x91, 0x01};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("Nested [[1]]: skipped %zu bytes (ok)", r.pos);
    }

    /* map16 with 1 entry: {1: 2} */
    {
        uint8_t data[] = {0xde, 0x00, 0x01, 0x01, 0x02};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("map16 {1:2}: skipped %zu bytes (ok)", r.pos);
    }

    /* map16 empty */
    {
        uint8_t data[] = {0xde, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("map16 empty: skipped (ok)");
    }

    /* map32 with 1 entry: {1: 2} */
    {
        uint8_t data[] = {0xdf, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("map32 {1:2}: skipped %zu bytes (ok)", r.pos);
    }

    /* map32 empty */
    {
        uint8_t data[] = {0xdf, 0x00, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("map32 empty: skipped (ok)");
    }

    /* array16 with 1 element */
    {
        uint8_t data[] = {0xdc, 0x00, 0x01, 0x05};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("array16 [5]: skipped %zu bytes (ok)", r.pos);
    }

    /* array16 empty */
    {
        uint8_t data[] = {0xdc, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("array16 empty: skipped (ok)");
    }

    /* array32 with 1 element */
    {
        uint8_t data[] = {0xdd, 0x00, 0x00, 0x00, 0x01, 0x05};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("array32 [5]: skipped %zu bytes (ok)", r.pos);
    }

    /* array32 empty */
    {
        uint8_t data[] = {0xdd, 0x00, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("array32 empty: skipped (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. skip_value — binary and wide string formats
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_skip_binary_and_wide_str) {
    cfgpack_reader_t r;

    LOG_SECTION("skip_value: bin8, bin16, bin32, str8, str16, str32");

    /* bin8 (0xc4) with 2 bytes payload */
    {
        uint8_t data[] = {0xc4, 0x02, 0xaa, 0xbb};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("bin8(2): skipped %zu bytes (ok)", r.pos);
    }

    /* bin16 (0xc5) with 2 bytes payload */
    {
        uint8_t data[] = {0xc5, 0x00, 0x02, 0xaa, 0xbb};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("bin16(2): skipped %zu bytes (ok)", r.pos);
    }

    /* bin32 (0xc6) with 2 bytes payload */
    {
        uint8_t data[] = {0xc6, 0x00, 0x00, 0x00, 0x02, 0xaa, 0xbb};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("bin32(2): skipped %zu bytes (ok)", r.pos);
    }

    /* str8 (0xd9) with 2 bytes payload */
    {
        uint8_t data[] = {0xd9, 0x02, 0x41, 0x42};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("str8(2): skipped %zu bytes (ok)", r.pos);
    }

    /* str16 (0xda) with 2 bytes payload */
    {
        uint8_t data[] = {0xda, 0x00, 0x02, 0x41, 0x42};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("str16(2): skipped %zu bytes (ok)", r.pos);
    }

    /* str32 (0xdb) with 2 bytes payload */
    {
        uint8_t data[] = {0xdb, 0x00, 0x00, 0x00, 0x02, 0x41, 0x42};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("str32(2): skipped %zu bytes (ok)", r.pos);
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. skip_value — truncation errors
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_skip_truncation) {
    cfgpack_reader_t r;

    LOG_SECTION("skip_value truncation errors");

    /* Empty buffer */
    {
        uint8_t data[] = {0};
        cfgpack_reader_init(&r, data, 0);
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("Empty buffer: ERR_DECODE (ok)");
    }

    /* fixstr body truncated */
    {
        uint8_t data[] = {0xa3, 'a'};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("fixstr truncated: ERR_DECODE (ok)");
    }

    /* bin8 (0xc4) — no length byte */
    {
        uint8_t data[] = {0xc4};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("bin8 no length: ERR_DECODE (ok)");
    }

    /* bin8 (0xc4) — body truncated */
    {
        uint8_t data[] = {0xc4, 0x05, 0x01};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("bin8 body truncated: ERR_DECODE (ok)");
    }

    /* bin16 (0xc5) — length truncated */
    {
        uint8_t data[] = {0xc5, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("bin16 length truncated: ERR_DECODE (ok)");
    }

    /* bin16 (0xc5) — body truncated */
    {
        uint8_t data[] = {0xc5, 0x00, 0x05, 0x01};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("bin16 body truncated: ERR_DECODE (ok)");
    }

    /* bin32 (0xc6) — length truncated */
    {
        uint8_t data[] = {0xc6, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("bin32 length truncated: ERR_DECODE (ok)");
    }

    /* bin32 (0xc6) — body truncated */
    {
        uint8_t data[] = {0xc6, 0x00, 0x00, 0x00, 0x05, 0x01};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("bin32 body truncated: ERR_DECODE (ok)");
    }

    /* str32 (0xdb) — body truncated */
    {
        uint8_t data[] = {0xdb, 0x00, 0x00, 0x00, 0x05, 0x41};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("str32 body truncated: ERR_DECODE (ok)");
    }

    /* float32 (0xca) — payload truncated */
    {
        uint8_t data[] = {0xca, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("f32 truncated: ERR_DECODE (ok)");
    }

    /* float64 (0xcb) — payload truncated */
    {
        uint8_t data[] = {0xcb, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("f64 truncated: ERR_DECODE (ok)");
    }

    /* uint8 (0xcc) — payload truncated */
    {
        uint8_t data[] = {0xcc};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("u8 truncated: ERR_DECODE (ok)");
    }

    /* uint16 (0xcd) — payload truncated */
    {
        uint8_t data[] = {0xcd, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("u16 truncated: ERR_DECODE (ok)");
    }

    /* array16 (0xdc) — length truncated */
    {
        uint8_t data[] = {0xdc, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("array16 length truncated: ERR_DECODE (ok)");
    }

    /* array32 (0xdd) — length truncated */
    {
        uint8_t data[] = {0xdd, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("array32 length truncated: ERR_DECODE (ok)");
    }

    /* map16 (0xde) — length truncated */
    {
        uint8_t data[] = {0xde, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("map16 length truncated: ERR_DECODE (ok)");
    }

    /* map32 (0xdf) — length truncated */
    {
        uint8_t data[] = {0xdf, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("map32 length truncated: ERR_DECODE (ok)");
    }

    /* Non-empty fixmap — child value truncated */
    {
        uint8_t data[] = {0x81, 0x01};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("fixmap child truncated: ERR_DECODE (ok)");
    }

    /* Non-empty fixarray — child truncated */
    {
        uint8_t data[] = {0x91};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("fixarray child truncated: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. skip_value — depth overflow, count overflow, and invalid type
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_skip_depth_and_overflow) {
    cfgpack_reader_t r;

    LOG_SECTION("skip_value depth overflow");

    /* 32 nested fixarray(1) exceeds CFGPACK_SKIP_MAX_DEPTH */
    {
        uint8_t data[33];
        int i;

        for (i = 0; i < 32; i++) {
            data[i] = 0x91;
        }
        data[32] = 0x01;
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("32 nested fixarrays: ERR_DECODE (ok)");
    }

    /* 31 nested fixarray(1) is within depth limit */
    {
        uint8_t data[32];
        int i;

        for (i = 0; i < 31; i++) {
            data[i] = 0x91;
        }
        data[31] = 0x01;
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_OK);
        CHECK(r.pos == sizeof(data));
        LOG("31 nested fixarrays: OK (ok)");
    }

    /* 32 nested fixmap(1) — each has key+value, value is next fixmap */
    {
        uint8_t data[33 * 2 - 1];
        int i;

        for (i = 0; i < 32; i++) {
            data[i * 2] = 0x81;
            data[i * 2 + 1] = 0x01;
        }
        data[64] = 0x02;
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("32 nested fixmaps: ERR_DECODE (ok)");
    }

    /* 32 nested array16(1) — each is 0xdc 0x00 0x01 */
    {
        uint8_t data[32 * 3 + 1];
        int i;

        for (i = 0; i < 32; i++) {
            data[i * 3] = 0xdc;
            data[i * 3 + 1] = 0x00;
            data[i * 3 + 2] = 0x01;
        }
        data[96] = 0x01;
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("32 nested array16s: ERR_DECODE (ok)");
    }

    /* 32 nested array32(1) — each is 0xdd 0x00 0x00 0x00 0x01 */
    {
        uint8_t data[32 * 5 + 1];
        int i;

        for (i = 0; i < 32; i++) {
            data[i * 5] = 0xdd;
            data[i * 5 + 1] = 0x00;
            data[i * 5 + 2] = 0x00;
            data[i * 5 + 3] = 0x00;
            data[i * 5 + 4] = 0x01;
        }
        data[160] = 0x01;
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("32 nested array32s: ERR_DECODE (ok)");
    }

    /* 32 nested map16(1) — each is 0xde 0x00 0x01, with key before child */
    {
        uint8_t data[32 * 4 + 1];
        int i;

        for (i = 0; i < 32; i++) {
            data[i * 4] = 0xde;
            data[i * 4 + 1] = 0x00;
            data[i * 4 + 2] = 0x01;
            data[i * 4 + 3] = 0x01;
        }
        data[128] = 0x02;
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("32 nested map16s: ERR_DECODE (ok)");
    }

    /* 32 nested map32(1) — each is 0xdf 0x00 0x00 0x00 0x01, with key */
    {
        uint8_t data[32 * 6 + 1];
        int i;

        for (i = 0; i < 32; i++) {
            data[i * 6] = 0xdf;
            data[i * 6 + 1] = 0x00;
            data[i * 6 + 2] = 0x00;
            data[i * 6 + 3] = 0x00;
            data[i * 6 + 4] = 0x01;
            data[i * 6 + 5] = 0x01;
        }
        data[192] = 0x02;
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("32 nested map32s: ERR_DECODE (ok)");
    }

    LOG_SECTION("skip_value map32 count overflow");

    /* map32 with count > UINT32_MAX/2 triggers overflow check */
    {
        uint8_t data[] = {0xdf, 0x80, 0x00, 0x00, 0x00};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("map32 count=0x80000000: ERR_DECODE (ok)");
    }

    LOG_SECTION("skip_value unknown/reserved type byte");

    /* 0xc1 is reserved in msgpack, hits default case */
    {
        uint8_t data[] = {0xc1};
        cfgpack_reader_init(&r, data, sizeof(data));
        CHECK(cfgpack_msgpack_skip_value(&r) == CFGPACK_ERR_DECODE);
        LOG("Reserved 0xc1: ERR_DECODE (ok)");
    }

    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. Encoder wrappers — encode_str_key and encode_int64 positive
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_encode_wrappers) {
    uint8_t storage[16];
    cfgpack_reader_t r;
    cfgpack_buf_t buf;

    LOG_SECTION("encode_str_key and encode_int64 positive delegation");

    /* encode_str_key is a thin wrapper around encode_str */
    {
        const uint8_t *ptr;
        uint32_t len;

        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_str_key(&buf, "key", 3) == CFGPACK_OK);
        CHECK(buf.len == 4);
        CHECK(storage[0] == (0xa0 | 3));

        cfgpack_reader_init(&r, storage, buf.len);
        CHECK(cfgpack_msgpack_decode_str(&r, &ptr, &len) == CFGPACK_OK);
        CHECK(len == 3);
        CHECK(memcmp(ptr, "key", 3) == 0);
        LOG("encode_str_key('key'): header=0x%02x len=%u (ok)", storage[0],
            len);
    }

    /* encode_int64 with positive value delegates to encode_uint64 */
    {
        uint64_t out;

        cfgpack_buf_init(&buf, storage, sizeof(storage));
        CHECK(cfgpack_msgpack_encode_int64(&buf, 42) == CFGPACK_OK);
        CHECK(buf.len == 1);
        CHECK(storage[0] == 42);

        cfgpack_reader_init(&r, storage, buf.len);
        CHECK(cfgpack_msgpack_decode_uint64(&r, &out) == CFGPACK_OK);
        CHECK(out == 42);
        LOG("encode_int64(42) -> fixint 0x2a (ok)");
    }

    /* encode_str header overflow with zero-capacity buffer */
    {
        uint8_t tiny;

        cfgpack_buf_init(&buf, &tiny, 0);
        CHECK(cfgpack_msgpack_encode_str(&buf, "x", 1) == CFGPACK_ERR_ENCODE);
        LOG("encode_str with 0 capacity: ERR_ENCODE (ok)");
    }

    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("decode_uint64_errors",
                                 test_decode_uint64_errors()) != TEST_OK);
    overall |= (test_case_result("decode_int64_positive_and_errors",
                                 test_decode_int64_positive_and_errors()) !=
                TEST_OK);
    overall |= (test_case_result("decode_int64_full_width_roundtrip",
                                 test_decode_int64_full_width_roundtrip()) !=
                TEST_OK);
    overall |= (test_case_result("decode_int16_byte_order",
                                 test_decode_int16_byte_order()) != TEST_OK);
    overall |= (test_case_result("skip_len32_overflow",
                                 test_skip_len32_overflow()) != TEST_OK);
    overall |= (test_case_result("encode_length_caps",
                                 test_encode_length_caps()) != TEST_OK);
    overall |= (test_case_result("decode_f32_errors",
                                 test_decode_f32_errors()) != TEST_OK);
    overall |= (test_case_result("decode_f64_errors",
                                 test_decode_f64_errors()) != TEST_OK);
    overall |= (test_case_result("decode_str_errors",
                                 test_decode_str_errors()) != TEST_OK);
    overall |= (test_case_result("decode_map_header_errors",
                                 test_decode_map_header_errors()) != TEST_OK);
    overall |= (test_case_result("skip_containers", test_skip_containers()) !=
                TEST_OK);
    overall |= (test_case_result("skip_binary_and_wide_str",
                                 test_skip_binary_and_wide_str()) != TEST_OK);
    overall |= (test_case_result("skip_truncation", test_skip_truncation()) !=
                TEST_OK);
    overall |= (test_case_result("skip_depth_and_overflow",
                                 test_skip_depth_and_overflow()) != TEST_OK);
    overall |= (test_case_result("encode_wrappers", test_encode_wrappers()) !=
                TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
