/**
 * @file main.c
 * @brief CFGPack Sensor Hub Example
 *
 * Demonstrates:
 * - Loading a compressed JSON schema from an external file
 * - Decompressing with heatshrink at runtime
 * - Parsing the JSON schema dynamically
 * - Configuring values using generic setter API by index
 * - Serializing to MessagePack and round-trip verification
 * - Exporting final config to JSON
 *
 * This simulates an embedded IoT sensor hub that receives its configuration
 * schema in compressed form (e.g., via OTA update) to minimize flash usage.
 */

#include <stdio.h>
#include <string.h>

#include "cfgpack/cfgpack.h"
#include "heatshrink_decoder.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Buffer Sizes (hardcoded maximums)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define MAX_ENTRIES         128
#define COMPRESSED_BUF_SIZE 4096
#define JSON_BUF_SIZE       8192    /* Max decompressed JSON schema size */
#define STORAGE_SIZE        2048    /* MessagePack storage buffer */
#define JSON_OUT_SIZE       8192    /* JSON export buffer */

/* ═══════════════════════════════════════════════════════════════════════════
 * Schema Storage (parsed from JSON)
 * ═══════════════════════════════════════════════════════════════════════════ */
static cfgpack_schema_t schema;
static cfgpack_entry_t entries[MAX_ENTRIES];
static cfgpack_fat_value_t defaults[MAX_ENTRIES];

/* ═══════════════════════════════════════════════════════════════════════════
 * Runtime Context
 * ═══════════════════════════════════════════════════════════════════════════ */
static cfgpack_ctx_t ctx;
static cfgpack_value_t values[MAX_ENTRIES];

/* String pool for runtime string values */
static char str_pool[1024];
static uint16_t str_offsets[MAX_ENTRIES];

/* ═══════════════════════════════════════════════════════════════════════════
 * Buffers
 * ═══════════════════════════════════════════════════════════════════════════ */
static uint8_t compressed_buf[COMPRESSED_BUF_SIZE];
static char json_buf[JSON_BUF_SIZE];
static uint8_t storage[STORAGE_SIZE];
static char json_out[JSON_OUT_SIZE];

/* Heatshrink decoder (static allocation per heatshrink_config.h) */
static heatshrink_decoder hsd;

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: Decompress heatshrink data
 * ═══════════════════════════════════════════════════════════════════════════ */
static int decompress_heatshrink(const uint8_t *input, size_t input_len,
                                  uint8_t *output, size_t output_cap,
                                  size_t *output_len) {
    size_t input_consumed = 0;
    size_t total_output = 0;
    size_t sink_count, poll_count;
    HSD_sink_res sink_res;
    HSD_poll_res poll_res;
    HSD_finish_res finish_res;

    heatshrink_decoder_reset(&hsd);

    /* Feed input and poll for output */
    while (input_consumed < input_len) {
        sink_res = heatshrink_decoder_sink(&hsd,
                                            (uint8_t *)(input + input_consumed),
                                            input_len - input_consumed,
                                            &sink_count);
        if (sink_res < 0) {
            fprintf(stderr, "Heatshrink sink error: %d\n", sink_res);
            return -1;
        }
        input_consumed += sink_count;

        /* Poll for decompressed output */
        do {
            poll_res = heatshrink_decoder_poll(&hsd,
                                                output + total_output,
                                                output_cap - total_output,
                                                &poll_count);
            if (poll_res < 0) {
                fprintf(stderr, "Heatshrink poll error: %d\n", poll_res);
                return -1;
            }
            total_output += poll_count;

            if (total_output > output_cap) {
                fprintf(stderr, "Output buffer overflow\n");
                return -1;
            }
        } while (poll_res == HSDR_POLL_MORE);
    }

    /* Finish decoding */
    do {
        finish_res = heatshrink_decoder_finish(&hsd);
        if (finish_res < 0) {
            fprintf(stderr, "Heatshrink finish error: %d\n", finish_res);
            return -1;
        }

        /* Poll remaining output */
        do {
            poll_res = heatshrink_decoder_poll(&hsd,
                                                output + total_output,
                                                output_cap - total_output,
                                                &poll_count);
            if (poll_res < 0) {
                fprintf(stderr, "Heatshrink poll error: %d\n", poll_res);
                return -1;
            }
            total_output += poll_count;

            if (total_output > output_cap) {
                fprintf(stderr, "Output buffer overflow\n");
                return -1;
            }
        } while (poll_res == HSDR_POLL_MORE);
    } while (finish_res == HSDR_FINISH_MORE);

    *output_len = total_output;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: Get type name string
 * ═══════════════════════════════════════════════════════════════════════════ */
static const char *type_name(cfgpack_type_t t) {
    switch (t) {
        case CFGPACK_TYPE_U8:   return "u8";
        case CFGPACK_TYPE_U16:  return "u16";
        case CFGPACK_TYPE_U32:  return "u32";
        case CFGPACK_TYPE_U64:  return "u64";
        case CFGPACK_TYPE_I8:   return "i8";
        case CFGPACK_TYPE_I16:  return "i16";
        case CFGPACK_TYPE_I32:  return "i32";
        case CFGPACK_TYPE_I64:  return "i64";
        case CFGPACK_TYPE_F32:  return "f32";
        case CFGPACK_TYPE_F64:  return "f64";
        case CFGPACK_TYPE_STR:  return "str";
        case CFGPACK_TYPE_FSTR: return "fstr";
        default:                return "???";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: Dump all entries (generic iteration)
 * ═══════════════════════════════════════════════════════════════════════════ */
#if 0  /* Unused but kept for reference/debugging */
static void dump_all_entries(const cfgpack_ctx_t *c) {
    printf("%-6s %-5s %-5s %s\n", "INDEX", "NAME", "TYPE", "VALUE");
    printf("------ ----- ----- ----------------------------------------\n");

    for (size_t i = 0; i < c->schema->entry_count; i++) {
        const cfgpack_entry_t *e = &c->schema->entries[i];
        cfgpack_value_t val;

        if (cfgpack_get(c, e->index, &val) != CFGPACK_OK)
            continue;

        printf("%-6u %-5s %-5s ", e->index, e->name, type_name(e->type));

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
            case CFGPACK_TYPE_I64:
                printf("%lld", (long long)val.v.i64);
                break;
            case CFGPACK_TYPE_F32:
                printf("%.6f", (double)val.v.f32);
                break;
            case CFGPACK_TYPE_F64:
                printf("%.6f", val.v.f64);
                break;
            case CFGPACK_TYPE_STR:
                printf("\"%.*s\"", (int)val.v.str.len, val.v.str.data);
                break;
            case CFGPACK_TYPE_FSTR:
                printf("\"%.*s\"", (int)val.v.fstr.len, val.v.fstr.data);
                break;
        }
        printf("\n");
    }
    printf("\n");
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: Hexdump
 * ═══════════════════════════════════════════════════════════════════════════ */
static void hexdump(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (len % 16 != 0)
        printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv) {
    cfgpack_err_t rc;
    cfgpack_parse_error_t parse_err;
    const char *schema_path = "schema.hs.bin";
    FILE *f;
    size_t compressed_len, json_len, storage_len, json_out_len;

    if (argc > 1)
        schema_path = argv[1];

    /* ═══════════════════════════════════════════════════════════════════════
     * 1. READ COMPRESSED SCHEMA FROM FILE
     * ═══════════════════════════════════════════════════════════════════════ */
    printf("=== CFGPack Sensor Hub Example ===\n\n");
    printf("Loading compressed schema from: %s\n", schema_path);

    f = fopen(schema_path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", schema_path);
        return 1;
    }
    compressed_len = fread(compressed_buf, 1, sizeof(compressed_buf), f);
    fclose(f);

    printf("Compressed size: %zu bytes\n", compressed_len);

    /* ═══════════════════════════════════════════════════════════════════════
     * 2. DECOMPRESS WITH HEATSHRINK
     * ═══════════════════════════════════════════════════════════════════════ */
    if (decompress_heatshrink(compressed_buf, compressed_len,
                               (uint8_t *)json_buf, sizeof(json_buf) - 1,
                               &json_len) != 0) {
        fprintf(stderr, "Decompression failed\n");
        return 1;
    }
    json_buf[json_len] = '\0';  /* NUL-terminate for JSON parser */

    printf("Decompressed size: %zu bytes\n", json_len);
    printf("Compression ratio: %.1f%%\n\n", 100.0 * compressed_len / json_len);

    /* ═══════════════════════════════════════════════════════════════════════
     * 3. PARSE JSON SCHEMA
     * ═══════════════════════════════════════════════════════════════════════ */
    rc = cfgpack_schema_parse_json(json_buf, json_len, &schema, entries,
                                    MAX_ENTRIES, defaults, &parse_err);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Schema parse error at line %zu: %s\n",
                parse_err.line, parse_err.message);
        return 1;
    }

    printf("Parsed schema: %s v%u (%zu entries)\n\n",
           schema.map_name, schema.version, schema.entry_count);

    /* ═══════════════════════════════════════════════════════════════════════
     * 4. INITIALIZE WITH DEFAULTS
     * ═══════════════════════════════════════════════════════════════════════ */
    rc = cfgpack_init(&ctx, &schema, values, MAX_ENTRIES, defaults,
                      str_pool, sizeof(str_pool),
                      str_offsets, MAX_ENTRIES);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Init failed: %d\n", rc);
        return 1;
    }

    printf("--- Initial config (first 10 entries) ---\n");
    printf("%-6s %-5s %-5s %s\n", "INDEX", "NAME", "TYPE", "VALUE");
    printf("------ ----- ----- ----------------------------------------\n");
    for (size_t i = 0; i < 10 && i < ctx.schema->entry_count; i++) {
        const cfgpack_entry_t *e = &ctx.schema->entries[i];
        cfgpack_value_t val;
        if (cfgpack_get(&ctx, e->index, &val) != CFGPACK_OK) continue;
        printf("%-6u %-5s %-5s ", e->index, e->name, type_name(e->type));
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
            case CFGPACK_TYPE_I64:
                printf("%lld", (long long)val.v.i64);
                break;
            case CFGPACK_TYPE_STR: {
                const char *str;
                uint16_t len;
                if (cfgpack_get_str(&ctx, e->index, &str, &len) == CFGPACK_OK)
                    printf("\"%.*s\"", (int)len, str);
                break;
            }
            case CFGPACK_TYPE_FSTR: {
                const char *str;
                uint8_t len;
                if (cfgpack_get_fstr(&ctx, e->index, &str, &len) == CFGPACK_OK)
                    printf("\"%.*s\"", (int)len, str);
                break;
            }
            default:
                break;
        }
        printf("\n");
    }
    printf("... (%zu more entries)\n\n", ctx.schema->entry_count - 10);

    /* ═══════════════════════════════════════════════════════════════════════
     * 5. MODIFY CONFIG (generic setter by index)
     * ═══════════════════════════════════════════════════════════════════════ */
    printf("--- Modifying config (generic API by index) ---\n");
    cfgpack_value_t val;

    /* Network: port = 8883 (MQTT over TLS) */
    val.type = CFGPACK_TYPE_U16;
    val.v.u64 = 8883;
    rc = cfgpack_set(&ctx, 5, &val);
    printf("  [5]  port  = 8883    %s\n", rc == CFGPACK_OK ? "OK" : "FAIL");

    /* MQTT: mtls = 1 (enable TLS) */
    val.type = CFGPACK_TYPE_U8;
    val.v.u64 = 1;
    rc = cfgpack_set(&ctx, 18, &val);
    printf("  [18] mtls  = 1       %s\n", rc == CFGPACK_OK ? "OK" : "FAIL");

    /* Device: dname = "hub01" */
    rc = cfgpack_set_fstr(&ctx, 57, "hub01");
    printf("  [57] dname = \"hub01\" %s\n", rc == CFGPACK_OK ? "OK" : "FAIL");

    /* Sensor 0: interval = 5000ms */
    val.type = CFGPACK_TYPE_U32;
    val.v.u64 = 5000;
    rc = cfgpack_set(&ctx, 27, &val);
    printf("  [27] s0iv  = 5000    %s\n", rc == CFGPACK_OK ? "OK" : "FAIL");

    /* Calibration: tcal = -25 (temperature offset) */
    val.type = CFGPACK_TYPE_I16;
    val.v.i64 = -25;
    rc = cfgpack_set(&ctx, 43, &val);
    printf("  [43] tcal  = -25     %s\n", rc == CFGPACK_OK ? "OK" : "FAIL");

    /* OTA: fwver = 101 (firmware version) */
    val.type = CFGPACK_TYPE_U16;
    val.v.u64 = 101;
    rc = cfgpack_set(&ctx, 53, &val);
    printf("  [53] fwver = 101     %s\n", rc == CFGPACK_OK ? "OK" : "FAIL");

    /* Power: batwn = 3400 (battery warning threshold mV) */
    val.type = CFGPACK_TYPE_U16;
    val.v.u64 = 3400;
    rc = cfgpack_set(&ctx, 65, &val);
    printf("  [65] batwn = 3400    %s\n\n", rc == CFGPACK_OK ? "OK" : "FAIL");

    /* ═══════════════════════════════════════════════════════════════════════
     * 6. SERIALIZE TO MESSAGEPACK
     * ═══════════════════════════════════════════════════════════════════════ */
    rc = cfgpack_pageout(&ctx, storage, sizeof(storage), &storage_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout failed: %d\n", rc);
        return 1;
    }

    printf("--- Serialized to MessagePack (%zu bytes) ---\n", storage_len);
    hexdump(storage, storage_len > 64 ? 64 : storage_len);
    if (storage_len > 64)
        printf("... (%zu more bytes)\n", storage_len - 64);
    printf("\n");

    /* ═══════════════════════════════════════════════════════════════════════
     * 7. ROUND-TRIP VERIFICATION
     * ═══════════════════════════════════════════════════════════════════════ */
    printf("--- Round-trip verification ---\n");

    /* Re-initialize context (simulates device reboot) */
    rc = cfgpack_init(&ctx, &schema, values, MAX_ENTRIES, defaults,
                      str_pool, sizeof(str_pool),
                      str_offsets, MAX_ENTRIES);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Re-init failed: %d\n", rc);
        return 1;
    }

    /* Load from storage (pagein) */
    rc = cfgpack_pagein_buf(&ctx, storage, storage_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pagein failed: %d\n", rc);
        return 1;
    }

    /* Verify modified values */
    int errors = 0;

    cfgpack_get(&ctx, 5, &val);
    if (val.v.u64 != 8883) { printf("  FAIL: port != 8883\n"); errors++; }
    else { printf("  OK: port = 8883\n"); }

    cfgpack_get(&ctx, 18, &val);
    if (val.v.u64 != 1) { printf("  FAIL: mtls != 1\n"); errors++; }
    else { printf("  OK: mtls = 1\n"); }

    cfgpack_get(&ctx, 57, &val);
    {
        const char *fstr;
        uint8_t fstr_len;
        if (cfgpack_get_fstr(&ctx, 57, &fstr, &fstr_len) != CFGPACK_OK ||
            fstr_len != 5 || memcmp(fstr, "hub01", 5) != 0) {
            printf("  FAIL: dname != \"hub01\"\n"); errors++;
        } else { printf("  OK: dname = \"hub01\"\n"); }
    }

    cfgpack_get(&ctx, 27, &val);
    if (val.v.u64 != 5000) { printf("  FAIL: s0iv != 5000\n"); errors++; }
    else { printf("  OK: s0iv = 5000\n"); }

    cfgpack_get(&ctx, 43, &val);
    if (val.v.i64 != -25) { printf("  FAIL: tcal != -25\n"); errors++; }
    else { printf("  OK: tcal = -25\n"); }

    cfgpack_get(&ctx, 53, &val);
    if (val.v.u64 != 101) { printf("  FAIL: fwver != 101\n"); errors++; }
    else { printf("  OK: fwver = 101\n"); }

    cfgpack_get(&ctx, 65, &val);
    if (val.v.u64 != 3400) { printf("  FAIL: batwn != 3400\n"); errors++; }
    else { printf("  OK: batwn = 3400\n"); }

    printf("\nRound-trip: %s (%d errors)\n\n",
           errors == 0 ? "PASSED" : "FAILED", errors);

    /* ═══════════════════════════════════════════════════════════════════════
     * 8. EXPORT SCHEMA WITH DEFAULTS TO JSON
     * ═══════════════════════════════════════════════════════════════════════ */
    rc = cfgpack_schema_write_json(&schema, defaults, json_out, sizeof(json_out),
                                    &json_out_len, &parse_err);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "JSON export failed: %d\n", rc);
        return 1;
    }

    f = fopen("config.json", "w");
    if (f) {
        fwrite(json_out, 1, json_out_len, f);
        fclose(f);
        printf("Exported final config to: config.json (%zu bytes)\n", json_out_len);
    }

    /* ═══════════════════════════════════════════════════════════════════════
     * 9. SUMMARY
     * ═══════════════════════════════════════════════════════════════════════ */
    printf("\n=== Summary ===\n");
    printf("Schema entries:     %zu\n", schema.entry_count);
    printf("Compressed schema:  %zu bytes\n", compressed_len);
    printf("Original schema:    %zu bytes\n", json_len);
    printf("Compression ratio:  %.1f%%\n", 100.0 * compressed_len / json_len);
    printf("MessagePack config: %zu bytes\n", storage_len);

    return errors;
}
