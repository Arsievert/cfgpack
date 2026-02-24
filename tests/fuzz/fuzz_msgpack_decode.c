/**
 * @file fuzz_msgpack_decode.c
 * @brief libFuzzer harness for the low-level MessagePack decoder.
 *
 * Exercises every decode function and skip_value against arbitrary
 * byte sequences. This tests the foundational codec layer that all
 * higher-level parsers depend on.
 */

#include "cfgpack/msgpack.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096) {
        return 0;
    }

    cfgpack_reader_t r;

    /* --- Decode as uint64 --- */
    cfgpack_reader_init(&r, data, size);
    {
        uint64_t out;
        (void)cfgpack_msgpack_decode_uint64(&r, &out);
    }

    /* --- Decode as int64 --- */
    cfgpack_reader_init(&r, data, size);
    {
        int64_t out;
        (void)cfgpack_msgpack_decode_int64(&r, &out);
    }

    /* --- Decode as f32 --- */
    cfgpack_reader_init(&r, data, size);
    {
        float out;
        (void)cfgpack_msgpack_decode_f32(&r, &out);
    }

    /* --- Decode as f64 --- */
    cfgpack_reader_init(&r, data, size);
    {
        double out;
        (void)cfgpack_msgpack_decode_f64(&r, &out);
    }

    /* --- Decode as string --- */
    cfgpack_reader_init(&r, data, size);
    {
        const uint8_t *ptr;
        uint32_t len;
        (void)cfgpack_msgpack_decode_str(&r, &ptr, &len);
    }

    /* --- Decode as map header --- */
    cfgpack_reader_init(&r, data, size);
    {
        uint32_t count;
        (void)cfgpack_msgpack_decode_map_header(&r, &count);
    }

    /* --- Skip value (handles all msgpack types) --- */
    cfgpack_reader_init(&r, data, size);
    (void)cfgpack_msgpack_skip_value(&r);

    /* --- Walk a sequence of skip_value calls to exercise nested structures --- */
    cfgpack_reader_init(&r, data, size);
    for (int i = 0; i < 16 && r.pos < r.len; ++i) {
        if (cfgpack_msgpack_skip_value(&r) != CFGPACK_OK) {
            break;
        }
    }

    return 0;
}
