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
 * @brief Create a simple MessagePack map for testing.
 *
 * Creates: {0: "test", 1: 42, 2: "hello"}
 */
static size_t create_test_msgpack(uint8_t *buf, size_t cap) {
    cfgpack_buf_t b;
    cfgpack_buf_init(&b, buf, cap);

    cfgpack_msgpack_encode_map_header(&b, 3);

    /* Key 0: schema name */
    cfgpack_msgpack_encode_uint_key(&b, 0);
    cfgpack_msgpack_encode_str(&b, "test", 4);

    /* Key 1: u8 value */
    cfgpack_msgpack_encode_uint_key(&b, 1);
    cfgpack_msgpack_encode_uint64(&b, 42);

    /* Key 2: string value */
    cfgpack_msgpack_encode_uint_key(&b, 2);
    cfgpack_msgpack_encode_str(&b, "hello", 5);

    return b.len;
}

/**
 * @brief Compress data with LZ4.
 */
static int compress_with_lz4(const uint8_t *input, size_t input_len,
                              uint8_t *output, size_t output_cap, size_t *output_len) {
    int result = LZ4_compress_default((const char *)input, (char *)output,
                                       (int)input_len, (int)output_cap);
    if (result <= 0) return -1;
    *output_len = (size_t)result;
    return 0;
}

/**
 * @brief Compress data with heatshrink.
 */
static int compress_with_heatshrink(const uint8_t *input, size_t input_len,
                                     uint8_t *output, size_t output_cap, size_t *output_len) {
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
        if (sink_res < 0) return -1;
        input_consumed += sink_count;

        do {
            poll_res = heatshrink_encoder_poll(&hs_encoder,
                                                output + total_output,
                                                output_cap - total_output,
                                                &poll_count);
            if (poll_res < 0) return -1;
            total_output += poll_count;
        } while (poll_res == HSER_POLL_MORE);
    }

    do {
        finish_res = heatshrink_encoder_finish(&hs_encoder);
        if (finish_res < 0) return -1;

        do {
            poll_res = heatshrink_encoder_poll(&hs_encoder,
                                                output + total_output,
                                                output_cap - total_output,
                                                &poll_count);
            if (poll_res < 0) return -1;
            total_output += poll_count;
        } while (poll_res == HSER_POLL_MORE);
    } while (finish_res == HSER_FINISH_MORE);

    *output_len = total_output;
    return 0;
}

/**
 * @brief Helper to set up a test context.
 */
static void setup_test_context(cfgpack_schema_t *schema, cfgpack_entry_t *entries,
                                cfgpack_ctx_t *ctx, cfgpack_value_t *values,
                                cfgpack_value_t *defaults, uint8_t *present) {
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

    cfgpack_init(ctx, schema, values, 2, defaults, present, 1);
}

TEST_CASE(test_lz4_basic) {
    LOG_SECTION("LZ4 basic compression/decompression roundtrip");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];
    size_t msgpack_len, compressed_len;
    cfgpack_err_t err;

    LOG("Creating test MessagePack data: {0: \"test\", 1: 42, 2: \"hello\"}");
    msgpack_len = create_test_msgpack(msgpack_buf, BUF_SIZE);
    CHECK(msgpack_len > 0);
    LOG("Created %zu bytes of MessagePack data", msgpack_len);
    LOG_HEX("Original msgpack", msgpack_buf, msgpack_len);

    LOG("Compressing with LZ4...");
    CHECK(compress_with_lz4(msgpack_buf, msgpack_len, compressed_buf, BUF_SIZE, &compressed_len) == 0);
    CHECK(compressed_len > 0);
    LOG("Compressed to %zu bytes (%.1f%% of original)", 
        compressed_len, (double)compressed_len / msgpack_len * 100);
    LOG_HEX("LZ4 compressed", compressed_buf, compressed_len);

    LOG("Setting up test context with schema: val(u8), str(str)");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);

    LOG("Calling cfgpack_pagein_lz4() to decompress and load...");
    err = cfgpack_pagein_lz4(&ctx, compressed_buf, compressed_len, msgpack_len);
    CHECK(err == CFGPACK_OK);
    LOG("Decompression successful");

    LOG("Verifying loaded values:");
    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.type == CFGPACK_TYPE_U8);
    CHECK(v.v.u64 == 42);
    LOG_VALUE("  Index 1 (val)", v);

    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_OK);
    CHECK(v.type == CFGPACK_TYPE_STR);
    CHECK(strcmp(v.v.str.data, "hello") == 0);
    LOG_VALUE("  Index 2 (str)", v);

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_lz4_null_args) {
    LOG_SECTION("LZ4 NULL argument handling");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];

    LOG("Setting up test context");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);

    LOG("Testing cfgpack_pagein_lz4(NULL, data, 10, 20)");
    CHECK(cfgpack_pagein_lz4(NULL, compressed_buf, 10, 20) == CFGPACK_ERR_DECODE);
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
    cfgpack_value_t defaults[2];
    uint8_t present[1];

    LOG("Setting up test context");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);

    LOG("Testing cfgpack_pagein_lz4() with decompressed_size=5000 (max is 4096)");
    CHECK(cfgpack_pagein_lz4(&ctx, compressed_buf, 10, 5000) == CFGPACK_ERR_BOUNDS);
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
    cfgpack_value_t defaults[2];
    uint8_t present[1];
    uint8_t garbage[32] = {0xFF, 0xFE, 0xFD, 0xFC, 0x00, 0x01, 0x02, 0x03};

    LOG("Setting up test context");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);

    LOG("Testing cfgpack_pagein_lz4() with garbage data:");
    LOG_HEX("Garbage input", garbage, sizeof(garbage));
    CHECK(cfgpack_pagein_lz4(&ctx, garbage, sizeof(garbage), 100) == CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for corrupted data");

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_heatshrink_basic) {
    LOG_SECTION("Heatshrink basic compression/decompression roundtrip");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];
    size_t msgpack_len, compressed_len;
    cfgpack_err_t err;

    LOG("Creating test MessagePack data: {0: \"test\", 1: 42, 2: \"hello\"}");
    msgpack_len = create_test_msgpack(msgpack_buf, BUF_SIZE);
    CHECK(msgpack_len > 0);
    LOG("Created %zu bytes of MessagePack data", msgpack_len);
    LOG_HEX("Original msgpack", msgpack_buf, msgpack_len);

    LOG("Compressing with heatshrink (window=8, lookahead=4)...");
    CHECK(compress_with_heatshrink(msgpack_buf, msgpack_len, compressed_buf, BUF_SIZE, &compressed_len) == 0);
    CHECK(compressed_len > 0);
    LOG("Compressed to %zu bytes (%.1f%% of original)", 
        compressed_len, (double)compressed_len / msgpack_len * 100);
    LOG_HEX("Heatshrink compressed", compressed_buf, compressed_len);

    LOG("Setting up test context with schema: val(u8), str(str)");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);

    LOG("Calling cfgpack_pagein_heatshrink() to decompress and load...");
    err = cfgpack_pagein_heatshrink(&ctx, compressed_buf, compressed_len);
    CHECK(err == CFGPACK_OK);
    LOG("Decompression successful");

    LOG("Verifying loaded values:");
    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.type == CFGPACK_TYPE_U8);
    CHECK(v.v.u64 == 42);
    LOG_VALUE("  Index 1 (val)", v);

    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_OK);
    CHECK(v.type == CFGPACK_TYPE_STR);
    CHECK(strcmp(v.v.str.data, "hello") == 0);
    LOG_VALUE("  Index 2 (str)", v);

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_heatshrink_null_args) {
    LOG_SECTION("Heatshrink NULL argument handling");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];

    LOG("Setting up test context");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);

    LOG("Testing cfgpack_pagein_heatshrink(NULL, data, 10)");
    CHECK(cfgpack_pagein_heatshrink(NULL, compressed_buf, 10) == CFGPACK_ERR_DECODE);
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
    cfgpack_value_t defaults[2];
    uint8_t present[1];

    LOG("Setting up test context");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);

    LOG("Testing cfgpack_pagein_heatshrink() with empty input (len=0)");
    uint8_t empty[1] = {0};
    CHECK(cfgpack_pagein_heatshrink(&ctx, empty, 0) == CFGPACK_ERR_DECODE);
    LOG("Correctly returned CFGPACK_ERR_DECODE for empty input");
    LOG("(heatshrink produces empty output, which is invalid msgpack)");

    LOG("Test completed successfully");
    return TEST_OK;
}

TEST_CASE(test_roundtrip_both_algorithms) {
    LOG_SECTION("Compare LZ4 and Heatshrink roundtrip");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];
    size_t msgpack_len, lz4_len, hs_len;

    LOG("Creating test MessagePack data");
    msgpack_len = create_test_msgpack(msgpack_buf, BUF_SIZE);
    CHECK(msgpack_len > 0);
    LOG("Original size: %zu bytes", msgpack_len);

    LOG("Compressing with both algorithms:");
    CHECK(compress_with_lz4(msgpack_buf, msgpack_len, compressed_buf, BUF_SIZE, &lz4_len) == 0);
    LOG("  LZ4: %zu bytes (%.1f%%)", lz4_len, (double)lz4_len / msgpack_len * 100);
    
    CHECK(compress_with_heatshrink(msgpack_buf, msgpack_len, scratch_buf, BUF_SIZE, &hs_len) == 0);
    LOG("  Heatshrink: %zu bytes (%.1f%%)", hs_len, (double)hs_len / msgpack_len * 100);

    LOG("Testing LZ4 roundtrip:");
    setup_test_context(&schema, entries, &ctx, values, defaults, present);
    CHECK(cfgpack_pagein_lz4(&ctx, compressed_buf, lz4_len, msgpack_len) == CFGPACK_OK);

    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 42);
    LOG("  LZ4 value verified: val = %llu", (unsigned long long)v.v.u64);

    LOG("Testing Heatshrink roundtrip:");
    memset(present, 0, sizeof(present));
    setup_test_context(&schema, entries, &ctx, values, defaults, present);
    CHECK(cfgpack_pagein_heatshrink(&ctx, scratch_buf, hs_len) == CFGPACK_OK);

    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 42);
    LOG("  Heatshrink value verified: val = %llu", (unsigned long long)v.v.u64);

    LOG("Both algorithms produced identical results");
    LOG("Test completed successfully");
    return TEST_OK;
}

int main(void) {
    int failures = 0;

    /* LZ4 tests */
    failures += test_case_result("lz4_basic", test_lz4_basic());
    failures += test_case_result("lz4_null_args", test_lz4_null_args());
    failures += test_case_result("lz4_size_too_large", test_lz4_size_too_large());
    failures += test_case_result("lz4_corrupted_data", test_lz4_corrupted_data());

    /* Heatshrink tests */
    failures += test_case_result("heatshrink_basic", test_heatshrink_basic());
    failures += test_case_result("heatshrink_null_args", test_heatshrink_null_args());
    failures += test_case_result("heatshrink_empty_input", test_heatshrink_empty_input());

    /* Combined test */
    failures += test_case_result("roundtrip_both_algorithms", test_roundtrip_both_algorithms());

    if (failures == 0) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    }
    return failures;
}
