/**
 * @file fuzz_decompress.c
 * @brief libFuzzer harness for cfgpack_pagein_lz4 and cfgpack_pagein_heatshrink.
 *
 * These wrappers are the most exposed untrusted-input path: a compressed
 * blob read straight from flash.  The first input byte selects the codec
 * and the scratch size, so both exact-fit and too-small scratch buffers
 * are reached (a full scratch buffer used to hang the heatshrink wrapper).
 * The remaining bytes are the compressed payload.
 *
 * Uses the same fixed six-entry schema as fuzz_pagein.c.
 */

#include "cfgpack/cfgpack.h"
#include "cfgpack/decompress.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_SCRATCH 4096

static cfgpack_schema_t schema;
static cfgpack_entry_t entries[6];
static uint8_t scratch[MAX_SCRATCH];

static void init_schema(void) {
    static const struct {
        const char *name;
        cfgpack_type_t type;
    } defs[6] = {{"u8v", CFGPACK_TYPE_U8},    {"i32v", CFGPACK_TYPE_I32},
                 {"f64v", CFGPACK_TYPE_F64},  {"strv", CFGPACK_TYPE_STR},
                 {"fstr", CFGPACK_TYPE_FSTR}, {"u64v", CFGPACK_TYPE_U64}};

    snprintf(schema.map_name, sizeof(schema.map_name), "fuzz");
    schema.version = 1;
    schema.entry_count = 6;
    schema.entries = entries;
    for (size_t i = 0; i < 6; ++i) {
        entries[i].index = (uint16_t)(i + 1);
        snprintf(entries[i].name, sizeof(entries[i].name), "%s", defs[i].name);
        entries[i].type = defs[i].type;
        entries[i].has_default = 0;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int inited = 0;
    cfgpack_value_t values[6];
    char str_pool[256];
    uint16_t str_offsets[2];
    cfgpack_ctx_t ctx;
    size_t scratch_cap;
    uint8_t mode;

    if (!inited) {
        init_schema();
        inited = 1;
    }
    if (size < 1 || size > MAX_SCRATCH) {
        return 0;
    }

    /* mode byte: bit 7 selects the codec; the low bits pick a scratch size
     * that ranges from 1 byte up to the whole buffer so boundary cases are
     * hit often. */
    mode = data[0];
    data++;
    size--;
    switch (mode & 0x03) {
    case 0: scratch_cap = 1 + (mode & 0x7c); break;        /* 1..125 */
    case 1: scratch_cap = size > 0 ? size : 1; break;      /* == input len */
    case 2: scratch_cap = 1 + ((mode & 0x7c) << 4); break; /* 1..1985 */
    default: scratch_cap = MAX_SCRATCH; break;
    }

    memset(values, 0, sizeof(values));
    if (cfgpack_init(&ctx, &schema, values, 6, str_pool, sizeof(str_pool),
                     str_offsets, 2) != CFGPACK_OK) {
        return 0;
    }

    if (mode & 0x80) {
        /* The decompressed size an LZ4 caller would read from its header:
         * use the scratch size so the exact-fit path is exercised. */
        (void)cfgpack_pagein_lz4(&ctx, data, size, scratch_cap, scratch,
                                 scratch_cap);
    } else {
        (void)cfgpack_pagein_heatshrink(&ctx, data, size, scratch, scratch_cap);
    }

    cfgpack_free(&ctx);
    return 0;
}
