/**
 * @file fuzz_parse_msgpack.c
 * @brief libFuzzer harness for the MessagePack schema parser.
 *
 * Feeds arbitrary bytes to cfgpack_schema_measure_msgpack() and
 * cfgpack_schema_parse_msgpack(), looking for crashes, ASan violations,
 * and undefined behavior.
 *
 * This is the highest-priority fuzz target because msgpack schema blobs
 * may be read from flash storage that could be corrupted.
 */

#include "cfgpack/cfgpack.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192) {
        return 0;
    }

    cfgpack_parse_error_t err;

    /* --- Exercise measure (pre-parse sizing scan) --- */
    cfgpack_schema_measure_t measure;
    (void)cfgpack_schema_measure_msgpack(data, size, &measure, &err);

    /* --- Exercise full parse --- */
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];

    cfgpack_parse_opts_t opts = {
        &schema,          entries,     128, values, str_pool,
        sizeof(str_pool), str_offsets, 128, &err,
    };
    (void)cfgpack_schema_parse_msgpack(data, size, &opts);

    return 0;
}
