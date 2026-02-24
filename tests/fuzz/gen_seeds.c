/**
 * @file gen_seeds.c
 * @brief Seed corpus generator for fuzz targets.
 *
 * Standalone hosted program that generates valid binary seed files for:
 *   - corpus_json/    (JSON schema blobs)
 *   - corpus_msgpack/ (msgpack schema blobs)
 *   - corpus_pagein/  (pageout'd config blobs)
 *   - corpus_decode/  (raw msgpack primitives)
 *
 * Build: make gen-seeds
 * Run:   build/out/gen_seeds   (writes files into tests/fuzz/corpus_xxx/)
 */
#include "cfgpack/cfgpack.h"
#include "cfgpack/msgpack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── helpers ────────────────────────────────────────────────────────────── */

static void write_file(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open %s\n", path);
        exit(1);
    }
    if (fwrite(data, 1, len, f) != len) {
        fprintf(stderr, "ERROR: short write to %s\n", path);
        fclose(f);
        exit(1);
    }
    fclose(f);
    printf("  wrote %s (%zu bytes)\n", path, len);
}

/* ── JSON corpus seeds ──────────────────────────────────────────────────── */

static void gen_json_seeds(void) {
    printf("corpus_json:\n");

    /* Seed 1: minimal valid schema */
    static const char json_minimal[] =
        "{\n"
        "  \"name\": \"min\",\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 42}\n"
        "  ]\n"
        "}";
    write_file("tests/fuzz/corpus_json/minimal.json", json_minimal,
               sizeof(json_minimal) - 1);

    /* Seed 2: schema with all types */
    static const char json_alltypes[] =
        "{\n"
        "  \"name\": \"all\",\n"
        "  \"version\": 2,\n"
        "  \"entries\": [\n"
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": "
        "255},\n"
        "    {\"index\": 2, \"name\": \"b\", \"type\": \"u16\", \"value\": "
        "1000},\n"
        "    {\"index\": 3, \"name\": \"c\", \"type\": \"u32\", \"value\": "
        "100000},\n"
        "    {\"index\": 4, \"name\": \"d\", \"type\": \"u64\", \"value\": "
        "999999},\n"
        "    {\"index\": 5, \"name\": \"e\", \"type\": \"i8\", \"value\": "
        "-10},\n"
        "    {\"index\": 6, \"name\": \"f\", \"type\": \"i16\", \"value\": "
        "-1000},\n"
        "    {\"index\": 7, \"name\": \"g\", \"type\": \"i32\", \"value\": "
        "-100000},\n"
        "    {\"index\": 8, \"name\": \"h\", \"type\": \"i64\", \"value\": "
        "-999999},\n"
        "    {\"index\": 9, \"name\": \"i\", \"type\": \"f32\", \"value\": "
        "3.14},\n"
        "    {\"index\": 10, \"name\": \"j\", \"type\": \"f64\", \"value\": "
        "2.718},\n"
        "    {\"index\": 11, \"name\": \"k\", \"type\": \"str\", \"value\": "
        "\"hello\"},\n"
        "    {\"index\": 12, \"name\": \"l\", \"type\": \"fstr\", \"value\": "
        "\"fix\"}\n"
        "  ]\n"
        "}";
    write_file("tests/fuzz/corpus_json/alltypes.json", json_alltypes,
               sizeof(json_alltypes) - 1);

    /* Seed 3: schema with null default */
    static const char json_nil[] = "{\n"
                                   "  \"name\": \"nil\",\n"
                                   "  \"version\": 1,\n"
                                   "  \"entries\": [\n"
                                   "    {\"index\": 1, \"name\": \"s\", "
                                   "\"type\": \"str\", \"value\": null}\n"
                                   "  ]\n"
                                   "}";
    write_file("tests/fuzz/corpus_json/nil_default.json", json_nil,
               sizeof(json_nil) - 1);
}

/* ── msgpack schema corpus seeds ────────────────────────────────────────── */

static void gen_msgpack_seeds(void) {
    printf("corpus_msgpack:\n");

    /* Parse the JSON "alltypes" schema, then write it as msgpack. */
    static const char json[] =
        "{"
        "\"name\":\"seed\","
        "\"version\":1,"
        "\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":42},"
        "{\"index\":2,\"name\":\"b\",\"type\":\"i32\",\"value\":-7},"
        "{\"index\":3,\"name\":\"c\",\"type\":\"f64\",\"value\":3.14},"
        "{\"index\":4,\"name\":\"d\",\"type\":\"str\",\"value\":\"hi\"},"
        "{\"index\":5,\"name\":\"e\",\"type\":\"fstr\",\"value\":\"fx\"}"
        "]}";

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;

    cfgpack_parse_opts_t opts = {
        &schema,          entries,     8, values, str_pool,
        sizeof(str_pool), str_offsets, 8, &err,
    };

    cfgpack_err_t rc = cfgpack_schema_parse_json(json, strlen(json), &opts);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "ERROR: parse JSON for msgpack seed: %s\n",
                err.message);
        exit(1);
    }

    /* Init a context so we can call cfgpack_schema_write_msgpack */
    cfgpack_ctx_t ctx;
    rc = cfgpack_init(&ctx, &schema, values, schema.entry_count, str_pool,
                      sizeof(str_pool), str_offsets, opts.str_offsets_count);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "ERROR: cfgpack_init for msgpack seed\n");
        exit(1);
    }

    /* Mark entries with defaults as present */
    for (size_t i = 0; i < schema.entry_count; ++i) {
        if (schema.entries[i].has_default) {
            cfgpack_presence_set(&ctx, i);
        }
    }

    uint8_t mp_buf[2048];
    size_t mp_len = 0;
    rc = cfgpack_schema_write_msgpack(&ctx, mp_buf, sizeof(mp_buf), &mp_len,
                                      &err);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "ERROR: cfgpack_schema_write_msgpack: %s\n",
                err.message);
        exit(1);
    }

    write_file("tests/fuzz/corpus_msgpack/schema_basic.bin", mp_buf, mp_len);
    cfgpack_free(&ctx);
}

/* ── pagein corpus seeds (pageout'd config blobs) ───────────────────────── */

static void gen_pagein_seeds(void) {
    printf("corpus_pagein:\n");

    /* Build the same 6-entry schema used in fuzz_pagein.c */
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[6];
    cfgpack_value_t values[6];
    char str_pool[256];
    uint16_t str_offsets[2]; /* str + fstr */

    memset(&schema, 0, sizeof(schema));
    snprintf(schema.map_name, sizeof(schema.map_name), "fuzz");
    schema.version = 1;
    schema.entry_count = 6;
    schema.entries = entries;

    memset(entries, 0, sizeof(entries));
    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "u8v");
    entries[0].type = CFGPACK_TYPE_U8;

    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "i32v");
    entries[1].type = CFGPACK_TYPE_I32;

    entries[2].index = 3;
    snprintf(entries[2].name, sizeof(entries[2].name), "f64v");
    entries[2].type = CFGPACK_TYPE_F64;

    entries[3].index = 4;
    snprintf(entries[3].name, sizeof(entries[3].name), "strv");
    entries[3].type = CFGPACK_TYPE_STR;

    entries[4].index = 5;
    snprintf(entries[4].name, sizeof(entries[4].name), "fstr");
    entries[4].type = CFGPACK_TYPE_FSTR;

    entries[5].index = 6;
    snprintf(entries[5].name, sizeof(entries[5].name), "u64v");
    entries[5].type = CFGPACK_TYPE_U64;

    cfgpack_ctx_t ctx;
    memset(values, 0, sizeof(values));
    cfgpack_err_t rc = cfgpack_init(&ctx, &schema, values, 6, str_pool,
                                    sizeof(str_pool), str_offsets, 2);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "ERROR: cfgpack_init for pagein seed\n");
        exit(1);
    }

    /* Set some values */
    cfgpack_set_u8(&ctx, 1, 200);
    cfgpack_set_i32(&ctx, 2, -42);
    cfgpack_set_f64(&ctx, 3, 1.5);
    cfgpack_set_str(&ctx, 4, "test");
    cfgpack_set_fstr(&ctx, 5, "fx");
    cfgpack_set_u64(&ctx, 6, 123456);

    /* Seed 1: all values present */
    uint8_t out[1024];
    size_t out_len = 0;
    rc = cfgpack_pageout(&ctx, out, sizeof(out), &out_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "ERROR: cfgpack_pageout seed 1\n");
        exit(1);
    }
    write_file("tests/fuzz/corpus_pagein/all_values.bin", out, out_len);

    /* Seed 2: only numeric values (no strings) */
    cfgpack_ctx_t ctx2;
    cfgpack_value_t values2[6];
    char str_pool2[256];
    uint16_t str_offsets2[2];
    memset(values2, 0, sizeof(values2));
    rc = cfgpack_init(&ctx2, &schema, values2, 6, str_pool2, sizeof(str_pool2),
                      str_offsets2, 2);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "ERROR: cfgpack_init for pagein seed 2\n");
        exit(1);
    }
    cfgpack_set_u8(&ctx2, 1, 0);
    cfgpack_set_i32(&ctx2, 2, 0);

    out_len = 0;
    rc = cfgpack_pageout(&ctx2, out, sizeof(out), &out_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "ERROR: cfgpack_pageout seed 2\n");
        exit(1);
    }
    write_file("tests/fuzz/corpus_pagein/numeric_only.bin", out, out_len);

    cfgpack_free(&ctx);
    cfgpack_free(&ctx2);
}

/* ── decode corpus seeds (raw msgpack primitives) ───────────────────────── */

static void gen_decode_seeds(void) {
    printf("corpus_decode:\n");
    uint8_t storage[256];
    cfgpack_buf_t buf;

    /* Seed 1: positive fixint (0x00) */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_uint64(&buf, 0);
    write_file("tests/fuzz/corpus_decode/uint_zero.bin", storage, buf.len);

    /* Seed 2: uint64 max */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_uint64(&buf, UINT64_MAX);
    write_file("tests/fuzz/corpus_decode/uint64_max.bin", storage, buf.len);

    /* Seed 3: negative fixint (-1) */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_int64(&buf, -1);
    write_file("tests/fuzz/corpus_decode/int_neg1.bin", storage, buf.len);

    /* Seed 4: int64 min */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_int64(&buf, INT64_MIN);
    write_file("tests/fuzz/corpus_decode/int64_min.bin", storage, buf.len);

    /* Seed 5: float32 */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_f32(&buf, 3.14f);
    write_file("tests/fuzz/corpus_decode/f32_pi.bin", storage, buf.len);

    /* Seed 6: float64 */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_f64(&buf, 2.718281828);
    write_file("tests/fuzz/corpus_decode/f64_e.bin", storage, buf.len);

    /* Seed 7: short string */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_str(&buf, "hello", 5);
    write_file("tests/fuzz/corpus_decode/str_hello.bin", storage, buf.len);

    /* Seed 8: empty string */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_str(&buf, "", 0);
    write_file("tests/fuzz/corpus_decode/str_empty.bin", storage, buf.len);

    /* Seed 9: small map with mixed values */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_map_header(&buf, 3);
    cfgpack_msgpack_encode_uint_key(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 42);
    cfgpack_msgpack_encode_uint_key(&buf, 2);
    cfgpack_msgpack_encode_int64(&buf, -7);
    cfgpack_msgpack_encode_uint_key(&buf, 3);
    cfgpack_msgpack_encode_str(&buf, "val", 3);
    write_file("tests/fuzz/corpus_decode/map_mixed.bin", storage, buf.len);

    /* Seed 10: nested map (map within a map value) */
    cfgpack_buf_init(&buf, storage, sizeof(storage));
    cfgpack_msgpack_encode_map_header(&buf, 1);
    cfgpack_msgpack_encode_uint_key(&buf, 0);
    cfgpack_msgpack_encode_map_header(&buf, 1);
    cfgpack_msgpack_encode_uint_key(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 99);
    write_file("tests/fuzz/corpus_decode/map_nested.bin", storage, buf.len);
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    printf("Generating fuzz seed corpora...\n\n");

    gen_json_seeds();
    printf("\n");
    gen_msgpack_seeds();
    printf("\n");
    gen_pagein_seeds();
    printf("\n");
    gen_decode_seeds();

    printf("\nDone.\n");
    return 0;
}
