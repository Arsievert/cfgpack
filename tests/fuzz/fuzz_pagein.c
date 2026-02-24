/**
 * @file fuzz_pagein.c
 * @brief libFuzzer harness for cfgpack_pagein_buf and cfgpack_peek_name.
 *
 * Exercises the runtime MessagePack deserialization path that loads
 * config values from a binary blob (e.g. from flash storage).
 * Uses a fixed valid schema -- we are fuzzing the input data, not
 * the schema setup.
 */

#include "cfgpack/cfgpack.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Fixed schema with a mix of types to exercise all decode paths.
 * Matches the kind of schema a real application would use. */
static cfgpack_schema_t schema;
static cfgpack_entry_t entries[6];

static void init_schema(void) {
    snprintf(schema.map_name, sizeof(schema.map_name), "fuzz");
    schema.version = 1;
    schema.entry_count = 6;
    schema.entries = entries;

    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "u8v");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 0;

    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "i32v");
    entries[1].type = CFGPACK_TYPE_I32;
    entries[1].has_default = 0;

    entries[2].index = 3;
    snprintf(entries[2].name, sizeof(entries[2].name), "f64v");
    entries[2].type = CFGPACK_TYPE_F64;
    entries[2].has_default = 0;

    entries[3].index = 4;
    snprintf(entries[3].name, sizeof(entries[3].name), "strv");
    entries[3].type = CFGPACK_TYPE_STR;
    entries[3].has_default = 0;

    entries[4].index = 5;
    snprintf(entries[4].name, sizeof(entries[4].name), "fstr");
    entries[4].type = CFGPACK_TYPE_FSTR;
    entries[4].has_default = 0;

    entries[5].index = 6;
    snprintf(entries[5].name, sizeof(entries[5].name), "u64v");
    entries[5].type = CFGPACK_TYPE_U64;
    entries[5].has_default = 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int inited = 0;
    if (!inited) {
        init_schema();
        inited = 1;
    }

    if (size > 4096) {
        return 0;
    }

    /* Fresh context for each iteration */
    cfgpack_value_t values[6];
    char str_pool[256];
    uint16_t str_offsets[2]; /* 2 string-type entries: strv, fstr */
    cfgpack_ctx_t ctx;

    memset(values, 0, sizeof(values));

    cfgpack_err_t rc = cfgpack_init(&ctx, &schema, values, 6, str_pool,
                                    sizeof(str_pool), str_offsets, 2);
    if (rc != CFGPACK_OK) {
        return 0;
    }

    /* Exercise pagein */
    (void)cfgpack_pagein_buf(&ctx, data, size);

    /* Exercise peek_name on the same data */
    char name_buf[64];
    (void)cfgpack_peek_name(data, size, name_buf, sizeof(name_buf));

    /* Also exercise pagein_remap with a small remap table */
    memset(values, 0, sizeof(values));
    memset(ctx.present, 0, sizeof(ctx.present));

    cfgpack_remap_entry_t remap[] = {
        {10, 1}, /* map old index 10 -> new index 1 */
        {20, 2}, /* map old index 20 -> new index 2 */
    };
    (void)cfgpack_pagein_remap(&ctx, data, size, remap, 2);

    cfgpack_free(&ctx);
    return 0;
}
