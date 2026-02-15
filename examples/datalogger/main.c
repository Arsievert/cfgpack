/**
 * @file main.c
 * @brief CFGPack Data Logger Example
 *
 * Demonstrates:
 * - Parsing schema from .map file
 * - Initializing config with defaults
 * - Reading/writing values using typed convenience functions
 * - Reading/writing values using generic cfgpack_value_t API
 * - Iterating all entries dynamically (type discovered at runtime)
 * - Serializing to MessagePack (pageout)
 * - Serializing to JSON for human-readable export
 * - Deserializing from MessagePack (pagein)
 * - Round-trip verification
 */

#include <stdio.h>
#include <string.h>

#include "cfgpack/cfgpack.h"

#define MAX_ENTRIES 16

/* Schema storage (parsed from .map file) */
static cfgpack_schema_t schema;
static cfgpack_entry_t entries[MAX_ENTRIES];

/* Runtime context */
static cfgpack_ctx_t ctx;
static cfgpack_value_t values[MAX_ENTRIES];

/* String pool for runtime string values */
static char str_pool[256];
static uint16_t str_offsets[MAX_ENTRIES];

/* Simulated flash storage */
static uint8_t storage[512];
static size_t storage_len = 0;

/* JSON output buffer */
static char json_buf[2048];

/* Scratch buffer for parsing .map file */
static char map_buf[2048];

/*---------------------------------------------------------------------------*/
static void hexdump(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    if (len % 16 != 0) {
        printf("\n");
    }
}

/*---------------------------------------------------------------------------*/
/* Helper to get type name string */
static const char *type_name(cfgpack_type_t t) {
    switch (t) {
    case CFGPACK_TYPE_U8: return "u8";
    case CFGPACK_TYPE_U16: return "u16";
    case CFGPACK_TYPE_U32: return "u32";
    case CFGPACK_TYPE_U64: return "u64";
    case CFGPACK_TYPE_I8: return "i8";
    case CFGPACK_TYPE_I16: return "i16";
    case CFGPACK_TYPE_I32: return "i32";
    case CFGPACK_TYPE_I64: return "i64";
    case CFGPACK_TYPE_F32: return "f32";
    case CFGPACK_TYPE_F64: return "f64";
    case CFGPACK_TYPE_STR: return "str";
    case CFGPACK_TYPE_FSTR: return "fstr";
    default: return "???";
    }
}

/*---------------------------------------------------------------------------*/
/**
 * Generic config dump - iterates all entries without knowing types at compile time.
 *
 * This demonstrates the power of the generic API: you can build tools like
 * debug dumps, CLI config editors, or remote config protocols that work with
 * ANY schema without hardcoding field names or types.
 */
static void dump_all_entries(const cfgpack_ctx_t *c) {
    printf("%-6s %-5s %-5s %s\n", "INDEX", "NAME", "TYPE", "VALUE");
    printf("------ ----- ----- ----------------------------------------\n");

    for (size_t i = 0; i < c->schema->entry_count; i++) {
        const cfgpack_entry_t *e = &c->schema->entries[i];
        cfgpack_value_t val;

        /* Get value using generic API - type discovered at runtime */
        if (cfgpack_get(c, e->index, &val) != CFGPACK_OK) {
            continue;
        }

        printf("%-6u %-5s %-5s ", e->index, e->name, type_name(e->type));

        /* Print value based on runtime type */
        switch (val.type) {
        case CFGPACK_TYPE_U8:
        case CFGPACK_TYPE_U16:
        case CFGPACK_TYPE_U32:
        case CFGPACK_TYPE_U64:
            printf("%llu", (unsigned long long)val.v.u64);
            break;
        case CFGPACK_TYPE_I8:
        case CFGPACK_TYPE_I16:
        case CFGPACK_TYPE_I32:
        case CFGPACK_TYPE_I64: printf("%lld", (long long)val.v.i64); break;
        case CFGPACK_TYPE_F32: printf("%.6f", (double)val.v.f32); break;
        case CFGPACK_TYPE_F64: printf("%.6f", val.v.f64); break;
        case CFGPACK_TYPE_STR: {
            const char *str;
            uint16_t len;
            if (cfgpack_get_str(c, e->index, &str, &len) == CFGPACK_OK) {
                printf("\"%.*s\"", (int)len, str);
            }
            break;
        }
        case CFGPACK_TYPE_FSTR: {
            const char *str;
            uint8_t len;
            if (cfgpack_get_fstr(c, e->index, &str, &len) == CFGPACK_OK) {
                printf("\"%.*s\"", (int)len, str);
            }
            break;
        }
        }
        printf("\n");
    }
    printf("\n");
}

/*---------------------------------------------------------------------------*/
static void print_config(const cfgpack_ctx_t *c) {
    uint32_t log_interval;
    uint16_t max_file_size, device_id, battery_warn;
    uint8_t en_temp, en_hum, en_pres, en_light, sleep_between;
    int16_t temp_off;
    int8_t hum_off;
    const char *prefix, *name;
    uint8_t prefix_len, name_len;
    cfgpack_value_t val;

    /* Field names are short (<=5 chars) to fit cfgpack's entry name limit:
     * intv=log_interval_ms, pfx=log_prefix, maxkb=max_file_size_kb,
     * ent/enh/enp/enl=enable_temp/humidity/pressure/light,
     * toff/hoff=temp_offset/humidity_offset, dname=device_name, did=device_id,
     * sleep=sleep_between, batwn=battery_warn_mv */

    /* Using generic cfgpack_get_by_name API for some fields */
    cfgpack_get_by_name(c, "intv", &val);
    log_interval = (uint32_t)val.v.u64;

    cfgpack_get_by_name(c, "did", &val);
    device_id = (uint16_t)val.v.u64;

    /* Using typed convenience functions for the rest */
    cfgpack_get_fstr_by_name(c, "pfx", &prefix, &prefix_len);
    cfgpack_get_u16_by_name(c, "maxkb", &max_file_size);

    cfgpack_get_u8_by_name(c, "ent", &en_temp);
    cfgpack_get_u8_by_name(c, "enh", &en_hum);
    cfgpack_get_u8_by_name(c, "enp", &en_pres);
    cfgpack_get_u8_by_name(c, "enl", &en_light);

    cfgpack_get_i16_by_name(c, "toff", &temp_off);
    cfgpack_get_i8_by_name(c, "hoff", &hum_off);

    cfgpack_get_fstr_by_name(c, "dname", &name, &name_len);

    cfgpack_get_u8_by_name(c, "sleep", &sleep_between);
    cfgpack_get_u16_by_name(c, "batwn", &battery_warn);

    printf("=== Data Logger Configuration ===\n");
    printf("Log interval:    %u ms\n", log_interval);
    printf("Log prefix:      %.*s\n", prefix_len, prefix);
    printf("Max file size:   %u KB\n", max_file_size);
    printf("Sensors:         temp=%u hum=%u pres=%u light=%u\n", en_temp,
           en_hum, en_pres, en_light);
    printf("Calibration:     temp_offset=%d, humidity_offset=%d\n", temp_off,
           hum_off);
    printf("Device:          %.*s (ID=%u)\n", name_len, name, device_id);
    printf("Power:           sleep=%u, battery_warn=%u mV\n", sleep_between,
           battery_warn);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
int main(int argc, char **argv) {
    cfgpack_err_t rc;
    cfgpack_parse_error_t parse_err;
    const char *map_path = "datalogger.map";

    if (argc > 1) {
        map_path = argv[1];
    }

    /* 1. Load and parse schema from .map file */
    printf("Loading schema from: %s\n", map_path);

    FILE *f = fopen(map_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", map_path);
        return 1;
    }
    size_t map_len = fread(map_buf, 1, sizeof(map_buf) - 1, f);
    fclose(f);
    map_buf[map_len] = '\0';

    cfgpack_parse_opts_t opts = {
        .out_schema = &schema,
        .entries = entries,
        .max_entries = MAX_ENTRIES,
        .values = values,
        .str_pool = str_pool,
        .str_pool_cap = sizeof(str_pool),
        .str_offsets = str_offsets,
        .str_offsets_count = MAX_ENTRIES,
        .err = &parse_err,
    };

    rc = cfgpack_parse_schema(map_buf, map_len, &opts);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Schema parse error at line %zu: %s\n", parse_err.line,
                parse_err.message);
        return 1;
    }
    printf("Loaded schema: %s (version %u, %zu entries)\n\n", schema.map_name,
           schema.version, schema.entry_count);

    /* 2. Initialize context with defaults */
    rc = cfgpack_init(&ctx, &schema, values, MAX_ENTRIES, str_pool,
                      sizeof(str_pool), str_offsets, MAX_ENTRIES);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Init failed: %d\n", rc);
        return 1;
    }

    printf("--- Initial config (defaults) ---\n");
    print_config(&ctx);

    /* 3. Modify some values - demonstrate both API styles */
    printf("--- Modifying values ---\n");

    /* Style 1: Typed convenience functions (concise, type-safe) */
    printf("  [typed API] intv = 5000\n");
    cfgpack_set_u32_by_name(&ctx, "intv", 5000);

    printf("  [typed API] dname = \"sensor-01\"\n");
    cfgpack_set_fstr_by_name(&ctx, "dname", "sensor-01");

    /* Style 2: Generic cfgpack_value_t API (flexible, explicit) */
    cfgpack_value_t val;

    printf("  [generic API] did = 42\n");
    val.type = CFGPACK_TYPE_U16;
    val.v.u64 = 42;
    cfgpack_set_by_name(&ctx, "did", &val);

    printf("  [generic API] enp = 1\n");
    val.type = CFGPACK_TYPE_U8;
    val.v.u64 = 1;
    cfgpack_set_by_name(&ctx, "enp", &val);

    printf("  [generic API] toff = -25\n");
    val.type = CFGPACK_TYPE_I16;
    val.v.i64 = -25;
    cfgpack_set_by_name(&ctx, "toff", &val);

    printf("\n");

    printf("--- After modifications ---\n");
    print_config(&ctx);

    /* 4. Generic API demo: dump all entries (type discovered at runtime) */
    printf("--- Generic API: dump all entries ---\n");
    dump_all_entries(&ctx);

    /* 5. Serialize to storage buffer (pageout) */
    rc = cfgpack_pageout(&ctx, storage, sizeof(storage), &storage_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout failed: %d\n", rc);
        return 1;
    }
    printf("--- Serialized to %zu bytes (MessagePack) ---\n", storage_len);
    hexdump(storage, storage_len);
    printf("\n");

    /* 6. Export schema with defaults to JSON (human-readable) */
    size_t json_len;
    rc = cfgpack_schema_write_json(&ctx, json_buf, sizeof(json_buf), &json_len,
                                   &parse_err);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "JSON export failed: %d\n", rc);
        return 1;
    }
    printf("--- Exported to JSON (%zu bytes) ---\n", json_len);
    printf("%s\n", json_buf);

    /* Write JSON to file */
    f = fopen("build/config.json", "w");
    if (f) {
        fwrite(json_buf, 1, json_len, f);
        fclose(f);
        printf("Written to: build/config.json\n\n");
    }

    /* 7. Re-initialize context (simulates device reboot) */

    /* Re-parse schema to restore defaults into values/str_pool */
    rc = cfgpack_parse_schema(map_buf, map_len, &opts);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Schema re-parse error: %s\n", parse_err.message);
        return 1;
    }

    rc = cfgpack_init(&ctx, &schema, values, MAX_ENTRIES, str_pool,
                      sizeof(str_pool), str_offsets, MAX_ENTRIES);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Re-init failed: %d\n", rc);
        return 1;
    }

    printf("--- After re-init (back to defaults) ---\n");
    print_config(&ctx);

    /* 8. Load from storage buffer (pagein) */
    rc = cfgpack_pagein_buf(&ctx, storage, storage_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pagein failed: %d\n", rc);
        return 1;
    }

    printf("--- After pagein (restored from storage) ---\n");
    print_config(&ctx);

    printf("Round-trip successful!\n");
    return 0;
}
