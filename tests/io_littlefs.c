/* LittleFS I/O wrapper tests for cfgpack.
 *
 * Uses a RAM-backed block device so tests run on desktop without
 * real flash hardware.
 */

#include "cfgpack/io_littlefs.h"

#include "cfgpack/cfgpack.h"

#include "test.h"

#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * RAM-backed block device
 * ───────────────────────────────────────────────────────────────────────────── */

#define BLOCK_SIZE 256
#define BLOCK_COUNT 64
#define LOOKAHEAD 16

static uint8_t lfs_ram[BLOCK_SIZE * BLOCK_COUNT];

static int ram_read(const struct lfs_config *c,
                    lfs_block_t block,
                    lfs_off_t off,
                    void *buffer,
                    lfs_size_t size) {
    (void)c;
    memcpy(buffer, &lfs_ram[block * BLOCK_SIZE + off], size);
    return (0);
}

static int ram_prog(const struct lfs_config *c,
                    lfs_block_t block,
                    lfs_off_t off,
                    const void *buffer,
                    lfs_size_t size) {
    (void)c;
    memcpy(&lfs_ram[block * BLOCK_SIZE + off], buffer, size);
    return (0);
}

static int ram_erase(const struct lfs_config *c, lfs_block_t block) {
    (void)c;
    memset(&lfs_ram[block * BLOCK_SIZE], 0xff, BLOCK_SIZE);
    return (0);
}

static int ram_sync(const struct lfs_config *c) {
    (void)c;
    return (0);
}

static uint8_t lfs_read_buf[BLOCK_SIZE];
static uint8_t lfs_prog_buf[BLOCK_SIZE];
static uint8_t lfs_lookahead_buf[LOOKAHEAD];

static const struct lfs_config lfs_cfg = {
    .read = ram_read,
    .prog = ram_prog,
    .erase = ram_erase,
    .sync = ram_sync,
    .read_size = BLOCK_SIZE,
    .prog_size = BLOCK_SIZE,
    .block_size = BLOCK_SIZE,
    .block_count = BLOCK_COUNT,
    .cache_size = BLOCK_SIZE,
    .lookahead_size = LOOKAHEAD,
    .block_cycles = -1,
    .read_buffer = lfs_read_buf,
    .prog_buffer = lfs_prog_buf,
    .lookahead_buffer = lfs_lookahead_buf,
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Test helpers
 * ───────────────────────────────────────────────────────────────────────────── */

static void make_schema(cfgpack_schema_t *schema,
                        cfgpack_entry_t *entries,
                        size_t n) {
    snprintf(schema->map_name, sizeof(schema->map_name), "test");
    schema->version = 1;
    schema->entry_count = n;
    schema->entries = entries;
    for (size_t i = 0; i < n; ++i) {
        entries[i].index = (uint16_t)(i + 1);
        snprintf(entries[i].name, sizeof(entries[i].name), "e%zu", i);
        entries[i].type = CFGPACK_TYPE_U8;
        entries[i].has_default = 0;
    }
}

static lfs_t lfs;

/* scratch must be >= cache_size + data */
static uint8_t scratch[BLOCK_SIZE + 1024];

static int mount_fresh(void) {
    lfs_format(&lfs, &lfs_cfg);
    return (lfs_mount(&lfs, &lfs_cfg));
}

static void unmount(void) {
    lfs_unmount(&lfs);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. Basic pageout/pagein roundtrip
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_roundtrip) {
    LOG_SECTION("LittleFS pageout/pagein roundtrip");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t values[2];
    cfgpack_value_t out;
    cfgpack_ctx_t ctx;
    cfgpack_err_t rc;

    make_schema(&schema, entries, 2);
    rc = cfgpack_init(&ctx, &schema, values, 2, NULL, 0, NULL, 0);
    CHECK(rc == CFGPACK_OK);

    cfgpack_set_u8(&ctx, 1, 42);
    cfgpack_set_u8(&ctx, 2, 99);
    LOG("Set values: e0=42, e1=99");

    CHECK(mount_fresh() == 0);

    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/config.bin", scratch,
                             sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Pageout to LittleFS succeeded");

    memset(values, 0, sizeof(values));
    memset(ctx.present, 0, sizeof(ctx.present));

    rc = cfgpack_pagein_lfs(&ctx, &lfs, "/config.bin", scratch,
                            sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Pagein from LittleFS succeeded");

    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 42);

    rc = cfgpack_get(&ctx, 2, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 99);
    LOG("Values verified: e0=42, e1=99");

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. File created after pageout
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_file_created) {
    LOG_SECTION("File exists after pageout");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    struct lfs_info info;
    cfgpack_ctx_t ctx;
    cfgpack_err_t rc;

    make_schema(&schema, entries, 1);
    cfgpack_init(&ctx, &schema, values, 1, NULL, 0, NULL, 0);
    cfgpack_set_u8(&ctx, 1, 7);

    CHECK(mount_fresh() == 0);

    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/check.bin", scratch,
                             sizeof(scratch));
    CHECK(rc == CFGPACK_OK);

    CHECK(lfs_stat(&lfs, "/check.bin", &info) == 0);
    CHECK(info.size > 0);
    LOG("File exists with size %lu", (unsigned long)info.size);

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. Pagein from nonexistent file
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_pagein_missing) {
    LOG_SECTION("Pagein from nonexistent file");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;

    make_schema(&schema, entries, 1);
    cfgpack_init(&ctx, &schema, values, 1, NULL, 0, NULL, 0);

    CHECK(mount_fresh() == 0);

    CHECK(cfgpack_pagein_lfs(&ctx, &lfs, "/nope.bin", scratch,
                             sizeof(scratch)) == CFGPACK_ERR_IO);
    LOG("Correctly returned CFGPACK_ERR_IO");

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. Scratch buffer too small for encode
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_pageout_scratch_too_small) {
    LOG_SECTION("Pageout with undersized scratch");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;
    cfgpack_err_t rc;

    /* data portion is 1 byte — too small for any msgpack output */
    uint8_t tiny[BLOCK_SIZE + 1];

    make_schema(&schema, entries, 1);
    cfgpack_init(&ctx, &schema, values, 1, NULL, 0, NULL, 0);
    cfgpack_set_u8(&ctx, 1, 1);

    CHECK(mount_fresh() == 0);

    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/tiny.bin", tiny, sizeof(tiny));
    CHECK(rc == CFGPACK_ERR_ENCODE);
    LOG("Correctly returned CFGPACK_ERR_ENCODE");

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. Scratch buffer too small for read
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_pagein_scratch_too_small) {
    LOG_SECTION("Pagein with undersized scratch");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;
    cfgpack_err_t rc;

    /* data portion is 1 byte — too small to hold the file */
    uint8_t tiny[BLOCK_SIZE + 1];

    make_schema(&schema, entries, 1);
    cfgpack_init(&ctx, &schema, values, 1, NULL, 0, NULL, 0);
    cfgpack_set_u8(&ctx, 1, 1);

    CHECK(mount_fresh() == 0);

    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/small.bin", scratch,
                             sizeof(scratch));
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_pagein_lfs(&ctx, &lfs, "/small.bin", tiny, sizeof(tiny));
    CHECK(rc == CFGPACK_ERR_IO);
    LOG("Correctly returned CFGPACK_ERR_IO");

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. Corrupted file data
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_pagein_corrupted) {
    LOG_SECTION("Pagein from corrupted file");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;

    make_schema(&schema, entries, 1);
    cfgpack_init(&ctx, &schema, values, 1, NULL, 0, NULL, 0);

    CHECK(mount_fresh() == 0);

    /* Write garbage directly to a LittleFS file */
    {
        struct lfs_file_config fcfg;
        uint8_t fcache[BLOCK_SIZE];
        lfs_file_t f;
        uint8_t garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFF};

        memset(&fcfg, 0, sizeof(fcfg));
        fcfg.buffer = fcache;
        CHECK(lfs_file_opencfg(&lfs, &f, "/bad.bin",
                               LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC,
                               &fcfg) == 0);
        CHECK(lfs_file_write(&lfs, &f, garbage, sizeof(garbage)) ==
              (lfs_ssize_t)sizeof(garbage));
        lfs_file_close(&lfs, &f);
    }

    CHECK(cfgpack_pagein_lfs(&ctx, &lfs, "/bad.bin", scratch,
                             sizeof(scratch)) == CFGPACK_ERR_CRC);
    LOG("Correctly returned CFGPACK_ERR_CRC");

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. Overwrite existing file
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_overwrite) {
    LOG_SECTION("Overwrite file with new values");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_value_t out;
    cfgpack_ctx_t ctx;
    cfgpack_err_t rc;

    make_schema(&schema, entries, 1);
    cfgpack_init(&ctx, &schema, values, 1, NULL, 0, NULL, 0);

    CHECK(mount_fresh() == 0);

    cfgpack_set_u8(&ctx, 1, 10);
    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/over.bin", scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("First pageout: e0=10");

    cfgpack_set_u8(&ctx, 1, 20);
    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/over.bin", scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Second pageout: e0=20");

    memset(values, 0, sizeof(values));
    memset(ctx.present, 0, sizeof(ctx.present));

    rc = cfgpack_pagein_lfs(&ctx, &lfs, "/over.bin", scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 20);
    LOG("Pagein reads latest value: e0=20");

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. Scratch smaller than cache_size
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_lfs_scratch_below_cache_size) {
    LOG_SECTION("Scratch smaller than cache_size");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;

    uint8_t tiny[BLOCK_SIZE]; /* exactly cache_size, no room for data */

    make_schema(&schema, entries, 1);
    cfgpack_init(&ctx, &schema, values, 1, NULL, 0, NULL, 0);
    cfgpack_set_u8(&ctx, 1, 1);

    CHECK(mount_fresh() == 0);

    CHECK(cfgpack_pageout_lfs(&ctx, &lfs, "/x.bin", tiny, sizeof(tiny)) ==
          CFGPACK_ERR_BOUNDS);
    CHECK(cfgpack_pagein_lfs(&ctx, &lfs, "/x.bin", tiny, sizeof(tiny)) ==
          CFGPACK_ERR_BOUNDS);
    LOG("Correctly returned CFGPACK_ERR_BOUNDS for both");

    unmount();
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test runner
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    int overall = TEST_OK;

    overall |= (test_case_result("lfs_roundtrip", test_lfs_roundtrip()) !=
                TEST_OK);
    overall |= (test_case_result("lfs_file_created", test_lfs_file_created()) !=
                TEST_OK);
    overall |= (test_case_result("lfs_pagein_missing",
                                 test_lfs_pagein_missing()) != TEST_OK);
    overall |= (test_case_result("lfs_pageout_scratch_too_small",
                                 test_lfs_pageout_scratch_too_small()) !=
                TEST_OK);
    overall |= (test_case_result("lfs_pagein_scratch_too_small",
                                 test_lfs_pagein_scratch_too_small()) !=
                TEST_OK);
    overall |= (test_case_result("lfs_pagein_corrupted",
                                 test_lfs_pagein_corrupted()) != TEST_OK);
    overall |= (test_case_result("lfs_overwrite", test_lfs_overwrite()) !=
                TEST_OK);
    overall |= (test_case_result("lfs_scratch_below_cache_size",
                                 test_lfs_scratch_below_cache_size()) !=
                TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
