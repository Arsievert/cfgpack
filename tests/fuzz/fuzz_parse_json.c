/**
 * @file fuzz_parse_json.c
 * @brief libFuzzer harness for the JSON schema parser.
 *
 * Feeds arbitrary bytes to cfgpack_schema_measure_json() and
 * cfgpack_schema_parse_json(), looking for crashes, ASan violations,
 * and undefined behavior.
 */

#include "cfgpack/cfgpack.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192) {
        return 0;
    }

    /* JSON parser works on a buffer with length; copy to ensure we
     * control the exact bounds (no stray NUL assumptions). */
    char buf[8193];
    memcpy(buf, data, size);
    buf[size] = '\0';

    cfgpack_parse_error_t err;

    /* --- Exercise measure (pre-parse sizing scan) --- */
    cfgpack_schema_measure_t measure;
    (void)cfgpack_schema_measure_json(buf, size, &measure, &err);

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
    (void)cfgpack_schema_parse_json(buf, size, &opts);

    return 0;
}
