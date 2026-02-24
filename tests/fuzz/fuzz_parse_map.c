/**
 * @file fuzz_parse_map.c
 * @brief libFuzzer harness for the .map schema parser.
 *
 * Feeds arbitrary bytes to cfgpack_schema_measure() and
 * cfgpack_parse_schema(), looking for crashes, ASan violations,
 * and undefined behavior.
 */

#include "cfgpack/cfgpack.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* The parser expects a NUL-terminated string. Copy into a local
     * buffer and ensure termination. Cap at 8 KiB to keep each
     * iteration fast. */
    if (size > 8192) {
        return 0;
    }

    char buf[8193];
    memcpy(buf, data, size);
    buf[size] = '\0';

    cfgpack_parse_error_t err;

    /* --- Exercise measure (pre-parse sizing scan) --- */
    cfgpack_schema_measure_t measure;
    (void)cfgpack_schema_measure(buf, size, &measure, &err);

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
    (void)cfgpack_parse_schema(buf, size, &opts);

    return 0;
}
