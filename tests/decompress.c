/**
 * @file decompress.c
 * @brief Tests for LZ4 and heatshrink decompression support.
 *
 * These tests use the encoder libraries to generate compressed data,
 * then verify that the decompression functions work correctly.
 */

#include "cfgpack/cfgpack.h"
#include "cfgpack/msgpack.h"
#include "test.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Include compression libraries for test data generation */
#include "lz4.h"
#include "heatshrink_encoder.h"

/* Test buffers */
#define BUF_SIZE 4096
static uint8_t msgpack_buf[BUF_SIZE];
static uint8_t compressed_buf[BUF_SIZE];
static uint8_t scratch_buf[BUF_SIZE];

/* Static heatshrink encoder for test data generation */
static heatshrink_encoder hs_encoder;

/**
 * @brief Create a larger MessagePack map for realistic compression testing.
 *
 * Creates a map with 15 entries matching sample.map schema:
 * - Key 0: schema name "demo"
 * - Keys 1-4: unsigned integers (u8, u16, u32, u64)
 * - Keys 5-8: signed integers (i8, i16, i32, i64)
 * - Keys 9-10: floats (f32, f64)
 * - Keys 11-15: strings with repetitive content (compression-friendly)
 *
 * This produces ~300 bytes of MessagePack data that compresses well.
 */
static size_t create_large_test_msgpack(uint8_t *buf, size_t cap) {
    cfgpack_buf_t b;
    cfgpack_buf_init(&b, buf, cap);

    /* Map with 16 entries (key 0 = schema name + 15 data entries) */
    cfgpack_msgpack_encode_map_header(&b, 16);

    /* Key 0: schema name */
    cfgpack_msgpack_encode_uint_key(&b, 0);
    cfgpack_msgpack_encode_str(&b, "demo", 4);

    /* Keys 1-4: unsigned integers */
    cfgpack_msgpack_encode_uint_key(&b, 1);
    cfgpack_msgpack_encode_uint64(&b, 255); /* u8 max */

    cfgpack_msgpack_encode_uint_key(&b, 2);
    cfgpack_msgpack_encode_uint64(&b, 1000); /* u16 */

    cfgpack_msgpack_encode_uint_key(&b, 3);
    cfgpack_msgpack_encode_uint64(&b, 100000); /* u32 */

    cfgpack_msgpack_encode_uint_key(&b, 4);
    cfgpack_msgpack_encode_uint64(&b, 9999999); /* u64 */

    /* Keys 5-8: signed integers */
    cfgpack_msgpack_encode_uint_key(&b, 5);
    cfgpack_msgpack_encode_int64(&b, -10); /* i8 */

    cfgpack_msgpack_encode_uint_key(&b, 6);
    cfgpack_msgpack_encode_int64(&b, -1000); /* i16 */

    cfgpack_msgpack_encode_uint_key(&b, 7);
    cfgpack_msgpack_encode_int64(&b, -100000); /* i32 */

    cfgpack_msgpack_encode_uint_key(&b, 8);
    cfgpack_msgpack_encode_int64(&b, -9999999); /* i64 */

    /* Keys 9-10: floats */
    cfgpack_msgpack_encode_uint_key(&b, 9);
    cfgpack_msgpack_encode_f32(&b, 3.14159f); /* f32 */

    cfgpack_msgpack_encode_uint_key(&b, 10);
    cfgpack_msgpack_encode_f64(&b, 2.718281828); /* f64 */

    /* Keys 11-15: strings with repetitive content for better compression */
    /* Using repetitive patterns that compress well */
    cfgpack_msgpack_encode_uint_key(&b, 11);
    cfgpack_msgpack_encode_str(&b, "hello world hello world hello world hello",
                               41);

    cfgpack_msgpack_encode_uint_key(&b, 12);
    cfgpack_msgpack_encode_str(&b, "config value config value config value cfg",
                               43);

    cfgpack_msgpack_encode_uint_key(&b, 13);
    cfgpack_msgpack_encode_str(&b, "fixed string!", 13); /* fstr */

    cfgpack_msgpack_encode_uint_key(&b, 14);
    cfgpack_msgpack_encode_str(&b, "test data test", 14); /* fstr */

    cfgpack_msgpack_encode_uint_key(&b, 15);
    cfgpack_msgpack_encode_str(
        &b, "the quick brown fox jumps over the lazy dog again and again", 59);

    return b.len;
}

/**
 * @brief Compress data with LZ4.
 */
static int compress_with_lz4(const uint8_t *input,
                             size_t input_len,
                             uint8_t *output,
                             size_t output_cap,
                             size_t *output_len) {
    int result = LZ4_compress_default((const char *)input, (char *)output,
                                      (int)input_len, (int)output_cap);
    if (result <= 0) {
        return -1;
    }
    *output_len = (size_t)result;
    return 0;
}

/**
 * @brief Compress data with heatshrink.
 */
static int compress_with_heatshrink(const uint8_t *input,
                                    size_t input_len,
                                    uint8_t *output,
                                    size_t output_cap,
                                    size_t *output_len) {
    size_t sink_count, poll_count;
    size_t input_consumed = 0;
    size_t total_output = 0;
    HSE_sink_res sink_res;
    HSE_poll_res poll_res;
    HSE_finish_res finish_res;

    heatshrink_encoder_reset(&hs_encoder);

    while (input_consumed < input_len) {
        sink_res = heatshrink_encoder_sink(&hs_encoder,
                                           (uint8_t *)(input + input_consumed),
                                           input_len - input_consumed,
                                           &sink_count);
        if (sink_res < 0) {
            return -1;
        }
        input_consumed += sink_count;

        do {
            poll_res = heatshrink_encoder_poll(&hs_encoder,
                                               output + total_output,
                                               output_cap - total_output,
                                               &poll_count);
            if (poll_res < 0) {
                return -1;
            }
            total_output += poll_count;
        } while (poll_res == HSER_POLL_MORE);
    }

    do {
        finish_res = heatshrink_encoder_finish(&hs_encoder);
        if (finish_res < 0) {
            return -1;
        }

        do {
            poll_res = heatshrink_encoder_poll(&hs_encoder,
                                               output + total_output,
                                               output_cap - total_output,
                                               &poll_count);
            if (poll_res < 0) {
                return -1;
            }
            total_output += poll_count;
        } while (poll_res == HSER_POLL_MORE);
    } while (finish_res == HSER_FINISH_MORE);

    *output_len = total_output;
    return 0;
}

/**
 * @brief Helper to set up a test context with 15 entries matching sample.map.
 */
static void setup_large_test_context(cfgpack_schema_t *schema,
                                     cfgpack_entry_t *entries,
                                     cfgpack_ctx_t *ctx,
                                     cfgpack_value_t *values,
                                     char *str_pool,
                                     size_t str_pool_cap,
                                     uint16_t *str_offsets,
                                     size_t str_offsets_count) {
    snprintf(schema->map_name, sizeof(schema->map_name), "demo");
    schema->version = 1;
    schema->entry_count = 15;
    schema->entries = entries;

    /* Unsigned integers */
    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "foo");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 0;

    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "bar");
    entries[1].type = CFGPACK_TYPE_U16;
    entries[1].has_default = 0;

    entries[2].index = 3;
    snprintf(entries[2].name, sizeof(entries[2].name), "baz");
    entries[2].type = CFGPACK_TYPE_U32;
    entries[2].has_default = 0;

    entries[3].index = 4;
    snprintf(entries[3].name, sizeof(entries[3].name), "qux");
    entries[3].type = CFGPACK_TYPE_U64;
    entries[3].has_default = 0;

    /* Signed integers */
    entries[4].index = 5;
    snprintf(entries[4].name, sizeof(entries[4].name), "qa");
    entries[4].type = CFGPACK_TYPE_I8;
    entries[4].has_default = 0;

    entries[5].index = 6;
    snprintf(entries[5].name, sizeof(entries[5].name), "qb");
    entries[5].type = CFGPACK_TYPE_I16;
    entries[5].has_default = 0;

    entries[6].index = 7;
    snprintf(entries[6].name, sizeof(entries[6].name), "qc");
    entries[6].type = CFGPACK_TYPE_I32;
    entries[6].has_default = 0;

    entries[7].index = 8;
    snprintf(entries[7].name, sizeof(entries[7].name), "qd");
    entries[7].type = CFGPACK_TYPE_I64;
    entries[7].has_default = 0;

    /* Floats */
    entries[8].index = 9;
    snprintf(entries[8].name, sizeof(entries[8].name), "fe");
    entries[8].type = CFGPACK_TYPE_F32;
    entries[8].has_default = 0;

    entries[9].index = 10;
    snprintf(entries[9].name, sizeof(entries[9].name), "fd");
    entries[9].type = CFGPACK_TYPE_F64;
    entries[9].has_default = 0;

    /* Strings */
    entries[10].index = 11;
    snprintf(entries[10].name, sizeof(entries[10].name), "s1");
    entries[10].type = CFGPACK_TYPE_STR;
    entries[10].has_default = 0;

    entries[11].index = 12;
    snprintf(entries[11].name, sizeof(entries[11].name), "s2");
    entries[11].type = CFGPACK_TYPE_STR;
    entries[11].has_default = 0;

    entries[12].index = 13;
    snprintf(entries[12].name, sizeof(entries[12].name), "fs1");
    entries[12].type = CFGPACK_TYPE_FSTR;
    entries[12].has_default = 0;

    entries[13].index = 14;
    snprintf(entries[13].name, sizeof(entries[13].name), "fs2");
    entries[13].type = CFGPACK_TYPE_FSTR;
    entries[13].has_default = 0;

    entries[14].index = 15;
    snprintf(entries[14].name, sizeof(entries[14].name), "s3");
    entries[14].type = CFGPACK_TYPE_STR;
    entries[14].has_default = 0;

    cfgpack_init(ctx, schema, values, 15, str_pool, str_pool_cap, str_offsets,
                 str_offsets_count);
}

/**
 * @brief Helper to set up a minimal test context (for error tests).
 */
static void setup_minimal_test_context(cfgpack_schema_t *schema,
                                       cfgpack_entry_t *entries,
                                       cfgpack_ctx_t *ctx,
                                       cfgpack_value_t *values,
                                       char *str_pool,
                                       size_t str_pool_cap,
                                       uint16_t *str_offsets,
                                       size_t str_offsets_count) {
    snprintf(schema->map_name, sizeof(schema->map_name), "test");
    schema->version = 1;
    schema->entry_count = 2;
    schema->entries = entries;

    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "val");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 0;

    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "str");
    entries[1].type = CFGPACK_TYPE_STR;
    entries[1].has_default = 0;

    cfgpack_init(ctx, schema, values, 2, str_pool, str_pool_cap, str_offsets,
                 str_offsets_count);
}

TEST_CASE(test_lz4_basic) {
    LOG_SECTION("LZ4 compression/decompression roundtrip (large dataset)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[15];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[15];
    char str_pool[512];      /* String pool for str/fstr values */
    uint16_t str_offsets[5]; /* 5 string entries: s1, s2, fs1, fs2, s3 */
    size_t msgpack_len, compressed_len;
    cfgpack_err_t err;

    LOG("Creating large test MessagePack data (15 entries, mixed types)");
    msgpack_len = create_large_test_msgpack(msgpack_buf, BUF_SIZE);
    CHECK(msgpack_len > 0);
    LOG("Created %zu bytes of MessagePack data", msgpack_len);
    LOG_HEX("Original msgpack (first 64 bytes)", msgpack_buf,
            msgpack_len > 64 ? 64 : msgpack_len);

    LOG("Compressing with LZ4...");
    CHECK(compress_with_lz4(msgpack_buf, msgpack_len, compressed_buf, BUF_SIZE,
                            &compressed_len) == 0);
    CHECK(compressed_len > 0);
    LOG("Compressed: %zu -> %zu bytes (%.1f%% of original)", msgpack_len,
        compressed_len, (double)compressed_len / msgpack_len * 100);
    CHECK(compressed_len <
          msgpack_len); /* Verify actual compression occurred */
    LOG("Compression verified: output smaller than input");
    LOG_HEX("LZ4 compressed (first 64 bytes)", compressed_buf,
            compressed_len > 64 ? 64 : compressed_len);

    LOG("Setting up test context with 15-entry schema");
    setup_large_test_context(&schema, entries, &ctx, values, str_pool,
                             sizeof(str_pool), str_offsets, 5);

    LOG("Calling cfgpack_pagein_lz4() to decompress and load...");
    err = cfgpack_pagein_lz4(&ctx, compressed_buf, compressed_len, msgpack_len);
    CHECK(err == CFGPACK_OK);
    LOG("Decompression and load successful");

    LOG("Verifying loaded values:");
    cfgpack_value_t v;
    const char *str_out;
    uint16_t str_len;
    uint8_t fstr_len;

    /* Check unsigned integers */
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 255);
    LOG("  [1] foo (u8) = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_OK && v.v.u64 == 1000);
    LOG("  [2] bar (u16) = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 3, &v) == CFGPACK_OK && v.v.u64 == 100000);
    LOG("  [3] baz (u32) = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 4, &v) == CFGPACK_OK && v.v.u64 == 9999999);
    LOG("  [4] qux (u64) = %" PRIu64, v.v.u64);

    /* Check signed integers */
    CHECK(cfgpack_get(&ctx, 5, &v) == CFGPACK_OK && v.v.i64 == -10);
    LOG("  [5] qa (i8) = %" PRId64, v.v.i64);
    CHECK(cfgpack_get(&ctx, 6, &v) == CFGPACK_OK && v.v.i64 == -1000);
    LOG("  [6] qb (i16) = %" PRId64, v.v.i64);
    CHECK(cfgpack_get(&ctx, 7, &v) == CFGPACK_OK && v.v.i64 == -100000);
    LOG("  [7] qc (i32) = %" PRId64, v.v.i64);
    CHECK(cfgpack_get(&ctx, 8, &v) == CFGPACK_OK && v.v.i64 == -9999999);
    LOG("  [8] qd (i64) = %" PRId64, v.v.i64);

    /* Check floats */
    CHECK(cfgpack_get(&ctx, 9, &v) == CFGPACK_OK);
    LOG("  [9] fe (f32) = %f", (double)v.v.f32);
    CHECK(cfgpack_get(&ctx, 10, &v) == CFGPACK_OK);
    LOG("  [10] fd (f64) = %f", v.v.f64);

    /* Check strings using cfgpack_get_str/cfgpack_get_fstr */
    CHECK(cfgpack_get_str(&ctx, 11, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 41);
    LOG("  [11] s1 (str) len=%u: \"%s\"", str_len, str_out);
    CHECK(cfgpack_get_str(&ctx, 12, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 43);
    LOG("  [12] s2 (str) len=%u: \"%s\"", str_len, str_out);
    CHECK(cfgpack_get_fstr(&ctx, 13, &str_out, &fstr_len) == CFGPACK_OK &&
          fstr_len == 13);
    LOG("  [13] fs1 (fstr) len=%u: \"%s\"", fstr_len, str_out);
    CHECK(cfgpack_get_fstr(&ctx, 14, &str_out, &fstr_len) == CFGPACK_OK &&
          fstr_len == 14);
    LOG("  [14] fs2 (fstr) len=%u: \"%s\"", fstr_len, str_out);
    CHECK(cfgpack_get_str(&ctx, 15, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 59);
    LOG("  [15] s3 (str) len=%u: \"%s\"", str_len, str_out);

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_lz4_null_args) {
    LOG_SECTION("LZ4 NULL argument handling");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    char str_pool[64];
    uint16_t str_offsets[1]; /* 1 string entry */

    LOG("Setting up minimal test context");
    setup_minimal_test_context(&schema, entries, &ctx, values, str_pool,
                               sizeof(str_pool), str_offsets, 1);

    LOG("Testing cfgpack_pagein_lz4(NULL, data, 10, 20)");
    CHECK(cfgpack_pagein_lz4(NULL, compressed_buf, 10, 20) ==
          CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for NULL ctx");

    LOG("Testing cfgpack_pagein_lz4(ctx, NULL, 10, 20)");
    CHECK(cfgpack_pagein_lz4(&ctx, NULL, 10, 20) == CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for NULL data");

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_lz4_size_too_large) {
    LOG_SECTION("LZ4 decompressed size exceeds buffer limit");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    char str_pool[64];
    uint16_t str_offsets[1];

    LOG("Setting up minimal test context");
    setup_minimal_test_context(&schema, entries, &ctx, values, str_pool,
                               sizeof(str_pool), str_offsets, 1);

    LOG("Testing cfgpack_pagein_lz4() with decompressed_size=5000 (max is "
        "4096)");
    CHECK(cfgpack_pagein_lz4(&ctx, compressed_buf, 10, 5000) ==
          CFGPACK_ERR_BOUNDS);
    LOG("Correctly returned CFGPACK_ERR_BOUNDS for size > PAGE_CAP");

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_lz4_corrupted_data) {
    LOG_SECTION("LZ4 corrupted/garbage data handling");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    char str_pool[64];
    uint16_t str_offsets[1];
    uint8_t garbage[32] = {0xff, 0xfe, 0xfd, 0xfc, 0x00, 0x01, 0x02, 0x03};

    LOG("Setting up minimal test context");
    setup_minimal_test_context(&schema, entries, &ctx, values, str_pool,
                               sizeof(str_pool), str_offsets, 1);

    LOG("Testing cfgpack_pagein_lz4() with garbage data:");
    LOG_HEX("Garbage input", garbage, sizeof(garbage));
    CHECK(cfgpack_pagein_lz4(&ctx, garbage, sizeof(garbage), 100) ==
          CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for corrupted data");

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_heatshrink_basic) {
    LOG_SECTION(
        "Heatshrink compression/decompression roundtrip (large dataset)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[15];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[15];
    char str_pool[512];
    uint16_t str_offsets[5];
    size_t msgpack_len, compressed_len;
    cfgpack_err_t err;

    LOG("Creating large test MessagePack data (15 entries, mixed types)");
    msgpack_len = create_large_test_msgpack(msgpack_buf, BUF_SIZE);
    CHECK(msgpack_len > 0);
    LOG("Created %zu bytes of MessagePack data", msgpack_len);
    LOG_HEX("Original msgpack (first 64 bytes)", msgpack_buf,
            msgpack_len > 64 ? 64 : msgpack_len);

    LOG("Compressing with heatshrink...");
    CHECK(compress_with_heatshrink(msgpack_buf, msgpack_len, compressed_buf,
                                   BUF_SIZE, &compressed_len) == 0);
    CHECK(compressed_len > 0);
    LOG("Compressed: %zu -> %zu bytes (%.1f%% of original)", msgpack_len,
        compressed_len, (double)compressed_len / msgpack_len * 100);
    CHECK(compressed_len <
          msgpack_len); /* Verify actual compression occurred */
    LOG("Compression verified: output smaller than input");
    LOG_HEX("Heatshrink compressed (first 64 bytes)", compressed_buf,
            compressed_len > 64 ? 64 : compressed_len);

    LOG("Setting up test context with 15-entry schema");
    setup_large_test_context(&schema, entries, &ctx, values, str_pool,
                             sizeof(str_pool), str_offsets, 5);

    LOG("Calling cfgpack_pagein_heatshrink() to decompress and load...");
    err = cfgpack_pagein_heatshrink(&ctx, compressed_buf, compressed_len);
    CHECK(err == CFGPACK_OK);
    LOG("Decompression and load successful");

    LOG("Verifying loaded values:");
    cfgpack_value_t v;
    const char *str_out;
    uint16_t str_len;
    uint8_t fstr_len;

    /* Check unsigned integers */
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 255);
    LOG("  [1] foo (u8) = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_OK && v.v.u64 == 1000);
    LOG("  [2] bar (u16) = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 3, &v) == CFGPACK_OK && v.v.u64 == 100000);
    LOG("  [3] baz (u32) = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 4, &v) == CFGPACK_OK && v.v.u64 == 9999999);
    LOG("  [4] qux (u64) = %" PRIu64, v.v.u64);

    /* Check signed integers */
    CHECK(cfgpack_get(&ctx, 5, &v) == CFGPACK_OK && v.v.i64 == -10);
    LOG("  [5] qa (i8) = %" PRId64, v.v.i64);
    CHECK(cfgpack_get(&ctx, 6, &v) == CFGPACK_OK && v.v.i64 == -1000);
    LOG("  [6] qb (i16) = %" PRId64, v.v.i64);
    CHECK(cfgpack_get(&ctx, 7, &v) == CFGPACK_OK && v.v.i64 == -100000);
    LOG("  [7] qc (i32) = %" PRId64, v.v.i64);
    CHECK(cfgpack_get(&ctx, 8, &v) == CFGPACK_OK && v.v.i64 == -9999999);
    LOG("  [8] qd (i64) = %" PRId64, v.v.i64);

    /* Check floats */
    CHECK(cfgpack_get(&ctx, 9, &v) == CFGPACK_OK);
    LOG("  [9] fe (f32) = %f", (double)v.v.f32);
    CHECK(cfgpack_get(&ctx, 10, &v) == CFGPACK_OK);
    LOG("  [10] fd (f64) = %f", v.v.f64);

    /* Check strings using cfgpack_get_str/cfgpack_get_fstr */
    CHECK(cfgpack_get_str(&ctx, 11, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 41);
    LOG("  [11] s1 (str) len=%u: \"%s\"", str_len, str_out);
    CHECK(cfgpack_get_str(&ctx, 12, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 43);
    LOG("  [12] s2 (str) len=%u: \"%s\"", str_len, str_out);
    CHECK(cfgpack_get_fstr(&ctx, 13, &str_out, &fstr_len) == CFGPACK_OK &&
          fstr_len == 13);
    LOG("  [13] fs1 (fstr) len=%u: \"%s\"", fstr_len, str_out);
    CHECK(cfgpack_get_fstr(&ctx, 14, &str_out, &fstr_len) == CFGPACK_OK &&
          fstr_len == 14);
    LOG("  [14] fs2 (fstr) len=%u: \"%s\"", fstr_len, str_out);
    CHECK(cfgpack_get_str(&ctx, 15, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 59);
    LOG("  [15] s3 (str) len=%u: \"%s\"", str_len, str_out);

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_heatshrink_null_args) {
    LOG_SECTION("Heatshrink NULL argument handling");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    char str_pool[64];
    uint16_t str_offsets[1];

    LOG("Setting up minimal test context");
    setup_minimal_test_context(&schema, entries, &ctx, values, str_pool,
                               sizeof(str_pool), str_offsets, 1);

    LOG("Testing cfgpack_pagein_heatshrink(NULL, data, 10)");
    CHECK(cfgpack_pagein_heatshrink(NULL, compressed_buf, 10) ==
          CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for NULL ctx");

    LOG("Testing cfgpack_pagein_heatshrink(ctx, NULL, 10)");
    CHECK(cfgpack_pagein_heatshrink(&ctx, NULL, 10) == CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for NULL data");

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_heatshrink_empty_input) {
    LOG_SECTION("Heatshrink empty input handling");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    char str_pool[64];
    uint16_t str_offsets[1];
    uint8_t empty[1] = {0};

    LOG("Setting up minimal test context");
    setup_minimal_test_context(&schema, entries, &ctx, values, str_pool,
                               sizeof(str_pool), str_offsets, 1);

    LOG("Testing cfgpack_pagein_heatshrink() with empty input (len=0)");
    /* heatshrink with 0 bytes will fail to decode any valid msgpack */
    CHECK(cfgpack_pagein_heatshrink(&ctx, empty, 0) == CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for empty input");

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_roundtrip_both_algorithms) {
    LOG_SECTION("Roundtrip comparison: LZ4 vs Heatshrink (large dataset)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[15];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[15];
    char str_pool[512];
    uint16_t str_offsets[5];
    size_t msgpack_len, lz4_len, hs_len;

    LOG("Creating large test MessagePack data");
    msgpack_len = create_large_test_msgpack(msgpack_buf, BUF_SIZE);
    LOG("Original size: %zu bytes", msgpack_len);
    LOG_HEX("Original msgpack (first 64 bytes)", msgpack_buf,
            msgpack_len > 64 ? 64 : msgpack_len);

    LOG("Compressing with LZ4...");
    CHECK(compress_with_lz4(msgpack_buf, msgpack_len, compressed_buf, BUF_SIZE,
                            &lz4_len) == 0);
    LOG("LZ4: %zu -> %zu bytes (%.1f%%)", msgpack_len, lz4_len,
        (double)lz4_len / msgpack_len * 100);

    LOG("Compressing with heatshrink...");
    CHECK(compress_with_heatshrink(msgpack_buf, msgpack_len, scratch_buf,
                                   BUF_SIZE, &hs_len) == 0);
    LOG("Heatshrink: %zu -> %zu bytes (%.1f%%)", msgpack_len, hs_len,
        (double)hs_len / msgpack_len * 100);

    LOG("Compression ratio comparison:");
    LOG("  Original:    %zu bytes", msgpack_len);
    LOG("  LZ4:         %zu bytes (%.1f%% of original)", lz4_len,
        (double)lz4_len / msgpack_len * 100);
    LOG("  Heatshrink:  %zu bytes (%.1f%% of original)", hs_len,
        (double)hs_len / msgpack_len * 100);

    /* Verify both actually compressed the data */
    CHECK(lz4_len < msgpack_len);
    CHECK(hs_len < msgpack_len);
    LOG("Both algorithms achieved compression (output < input)");

    LOG("Testing LZ4 decompression and load...");
    setup_large_test_context(&schema, entries, &ctx, values, str_pool,
                             sizeof(str_pool), str_offsets, 5);
    CHECK(cfgpack_pagein_lz4(&ctx, compressed_buf, lz4_len, msgpack_len) ==
          CFGPACK_OK);
    cfgpack_value_t v;
    const char *str_out;
    uint16_t str_len;

    /* Check unsigned integers */
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 255);
    CHECK(cfgpack_get_str(&ctx, 11, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 41);
    LOG("LZ4 decompression verified");

    LOG("Testing heatshrink decompression and load...");
    memset(values, 0, sizeof(values));
    memset(ctx.present, 0, sizeof(ctx.present));
    memset(str_pool, 0, sizeof(str_pool));
    setup_large_test_context(&schema, entries, &ctx, values, str_pool,
                             sizeof(str_pool), str_offsets, 5);
    CHECK(cfgpack_pagein_heatshrink(&ctx, scratch_buf, hs_len) == CFGPACK_OK);
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 255);
    CHECK(cfgpack_get_str(&ctx, 11, &str_out, &str_len) == CFGPACK_OK &&
          str_len == 41);
    LOG("Heatshrink decompression verified");

    LOG("Test completed successfully");
    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("lz4_basic", test_lz4_basic()) != TEST_OK);
    overall |= (test_case_result("lz4_null_args", test_lz4_null_args()) !=
                TEST_OK);
    overall |= (test_case_result("lz4_size_too_large",
                                 test_lz4_size_too_large()) != TEST_OK);
    overall |= (test_case_result("lz4_corrupted_data",
                                 test_lz4_corrupted_data()) != TEST_OK);
    overall |= (test_case_result("heatshrink_basic", test_heatshrink_basic()) !=
                TEST_OK);
    overall |= (test_case_result("heatshrink_null_args",
                                 test_heatshrink_null_args()) != TEST_OK);
    overall |= (test_case_result("heatshrink_empty_input",
                                 test_heatshrink_empty_input()) != TEST_OK);
    overall |= (test_case_result("roundtrip_both_algorithms",
                                 test_roundtrip_both_algorithms()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return overall;
}
