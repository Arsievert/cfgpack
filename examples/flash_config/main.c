/**
 * @file main.c
 * @brief CFGPack Flash Config Example — LittleFS + LZ4 + Schema Migration
 *
 * Demonstrates:
 * - LittleFS pageout/pagein with a RAM-backed block device
 * - LZ4-compressed msgpack binary schemas (built by cfgpack-schema-pack
 *   and cfgpack-compress)
 * - Schema migration v1 -> v2 using cfgpack_pagein_remap()
 *   with all five scenarios: KEEP, WIDEN, MOVE, REMOVE, ADD
 * - Composable I/O: manual LFS read + pagein_remap for migration
 *   (cfgpack_pagein_lfs wraps pagein_buf only, not pagein_remap)
 *
 * Scenario: Industrial sensor node storing config on SPI NOR flash via
 * LittleFS. Monitors temperature, humidity, and pressure with configurable
 * thresholds and reporting intervals. Firmware upgrade migrates config
 * from v1 (19 entries) to v2 (21 entries).
 *
 * The LFS scratch buffer is split internally: the first cache_size bytes
 * serve as the LittleFS file cache (via lfs_file_opencfg), and the
 * remainder holds the serialized data. This avoids lfs_malloc.
 *
 * Build pipeline (handled by Makefile):
 *   .map --(cfgpack-schema-pack)--> .msgpack --(cfgpack-compress lz4)--> .msgpack.lz4
 */

#include "cfgpack/cfgpack.h"
#include "cfgpack/io_littlefs.h"

#include "lz4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * RAM-backed LittleFS block device
 *
 * On a real device this would be SPI NOR flash with hardware callbacks.
 * The RAM-backed version lets the example run on any desktop.
 * ═══════════════════════════════════════════════════════════════════════════ */

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

static lfs_t lfs;

/* ═══════════════════════════════════════════════════════════════════════════
 * LFS scratch buffer
 *
 * cfgpack_pageout_lfs / cfgpack_pagein_lfs split this buffer internally:
 *   [0 .. cache_size-1]       LittleFS file cache (for lfs_file_opencfg)
 *   [cache_size .. end]       serialized config data
 *
 * Size = cache_size (256) + room for encoded config (~512 bytes generous).
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LFS_SCRATCH_SIZE (BLOCK_SIZE + 1024)
static uint8_t lfs_scratch[LFS_SCRATCH_SIZE];

/* ═══════════════════════════════════════════════════════════════════════════
 * v1 -> v2 remap table
 *
 * Only MOVE entries are listed. Implicit migrations:
 *   KEEP:   indices 1-4, 6, 10, 14, 17-19 exist in both schemas
 *   WIDEN:  idx 6 (tmint u16->u32), idx 10 (hmint u16->u32)
 *   REMOVE: idx 5 (tmen), 9 (hmen), 13 (pren) absent in v2
 *   ADD:    idx 30-34 (smask, loglv, ntph, tzone, batch) get defaults
 * ═══════════════════════════════════════════════════════════════════════════ */

static const cfgpack_remap_entry_t v1_to_v2_remap[] = {
    {7, 40},  /* tmhi: temp high threshold   -> alarm block */
    {8, 41},  /* tmlo: temp low threshold    -> alarm block */
    {11, 42}, /* hmhi: humid high threshold  -> alarm block */
    {12, 43}, /* hmlo: humid low threshold   -> alarm block */
    {15, 44}, /* prhi: press high threshold  -> alarm block */
    {16, 45}, /* prlo: press low threshold   -> alarm block */
};
#define REMAP_COUNT (sizeof(v1_to_v2_remap) / sizeof(v1_to_v2_remap[0]))

/* ═══════════════════════════════════════════════════════════════════════════
 * Heap-allocated schema buffers (measure -> malloc -> parse)
 * ═══════════════════════════════════════════════════════════════════════════ */

static cfgpack_entry_t *entries;
static cfgpack_value_t *values;
static char *str_pool;
static uint16_t *str_offsets;

/* ═══════════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

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

static void dump_entries(const cfgpack_ctx_t *c) {
    printf("  %-5s %-5s %-4s %s\n", "IDX", "NAME", "TYPE", "VALUE");
    printf("  ----- ----- ---- ----------------------------------------\n");

    for (size_t i = 0; i < c->schema->entry_count; i++) {
        const cfgpack_entry_t *e = &c->schema->entries[i];
        cfgpack_value_t val;

        if (cfgpack_get(c, e->index, &val) != CFGPACK_OK) {
            continue;
        }

        printf("  %-5u %-5s %-4s ", e->index, e->name, type_name(e->type));

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
        case CFGPACK_TYPE_F32: printf("%.2f", (double)val.v.f32); break;
        case CFGPACK_TYPE_F64: printf("%.2f", val.v.f64); break;
        case CFGPACK_TYPE_STR: {
            const char *s;
            uint16_t len;
            if (cfgpack_get_str(c, e->index, &s, &len) == CFGPACK_OK) {
                printf("\"%.*s\"", (int)len, s);
            }
            break;
        }
        case CFGPACK_TYPE_FSTR: {
            const char *s;
            uint8_t len;
            if (cfgpack_get_fstr(c, e->index, &s, &len) == CFGPACK_OK) {
                printf("\"%.*s\"", (int)len, s);
            }
            break;
        }
        }
        printf("\n");
    }
    printf("\n");
}

static void print_memory_report(const char *label,
                                const cfgpack_schema_measure_t *m) {
    size_t entries_bytes = m->entry_count * sizeof(cfgpack_entry_t);
    size_t values_bytes = m->entry_count * sizeof(cfgpack_value_t);
    size_t pool_bytes = m->str_pool_size;
    size_t str_off_count = m->str_count + m->fstr_count;
    size_t offsets_bytes = str_off_count * sizeof(uint16_t);

    size_t total = entries_bytes + values_bytes + pool_bytes + offsets_bytes;

    printf("  Memory report: %s\n", label);
    printf("    entries[%zu]       %4zu B  (%zu x %zu)\n", m->entry_count,
           entries_bytes, m->entry_count, sizeof(cfgpack_entry_t));
    printf("    values[%zu]        %4zu B  (%zu x %zu)\n", m->entry_count,
           values_bytes, m->entry_count, sizeof(cfgpack_value_t));
    printf("    str_pool           %4zu B  (%zu str x %d + %zu fstr x %d)\n",
           pool_bytes, m->str_count, CFGPACK_STR_MAX + 1, m->fstr_count,
           CFGPACK_FSTR_MAX + 1);
    printf("    str_offsets        %4zu B  (%zu x %zu)\n", offsets_bytes,
           str_off_count, sizeof(uint16_t));
    printf("    ────────────────────────\n");
    printf("    TOTAL              %4zu B\n\n", total);
}

static void free_buffers(void) {
    free(entries);
    free(values);
    free(str_pool);
    free(str_offsets);
    entries = NULL;
    values = NULL;
    str_pool = NULL;
    str_offsets = NULL;
}

static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    long sz;
    uint8_t *buf;

    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return (NULL);
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        fprintf(stderr, "Empty or unreadable: %s\n", path);
        return (NULL);
    }
    buf = malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "malloc failed for %s\n", path);
        return (NULL);
    }
    *out_len = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    return (buf);
}

static uint8_t *read_lz4_file(const char *path,
                              size_t *out_len,
                              size_t *out_compressed_len) {
    size_t file_len;
    uint8_t *file_data = read_file(path, &file_len);
    const uint8_t *compressed;
    size_t compressed_len;
    uint8_t *decompressed;
    uint32_t orig_size;
    int result;

    if (!file_data) {
        return (NULL);
    }

    if (file_len < 4) {
        fprintf(stderr, "LZ4 file too small (no header): %s\n", path);
        free(file_data);
        return (NULL);
    }

    orig_size = (uint32_t)file_data[0] | ((uint32_t)file_data[1] << 8) |
                ((uint32_t)file_data[2] << 16) | ((uint32_t)file_data[3] << 24);

    compressed = file_data + 4;
    compressed_len = file_len - 4;

    decompressed = malloc(orig_size);
    if (!decompressed) {
        fprintf(stderr, "malloc failed for decompression (%u bytes)\n",
                orig_size);
        free(file_data);
        return (NULL);
    }

    result = LZ4_decompress_safe((const char *)compressed, (char *)decompressed,
                                 (int)compressed_len, (int)orig_size);
    free(file_data);

    if (result < 0) {
        fprintf(stderr, "LZ4 decompression failed for %s (error %d)\n", path,
                result);
        free(decompressed);
        return (NULL);
    }

    *out_len = (size_t)orig_size;
    *out_compressed_len = file_len;
    return (decompressed);
}

/**
 * Load a msgpack binary schema using measure -> malloc -> parse -> init.
 */
static int load_msgpack_schema(const uint8_t *mp_data,
                               size_t mp_len,
                               const char *label,
                               cfgpack_schema_t *schema,
                               cfgpack_ctx_t *ctx,
                               cfgpack_schema_measure_t *out_m) {
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t perr;
    cfgpack_parse_opts_t opts;
    size_t str_off_count;
    cfgpack_err_t rc;

    rc = cfgpack_schema_measure_msgpack(mp_data, mp_len, &m, &perr);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Measure error (%s): %s\n", label, perr.message);
        return (-1);
    }

    str_off_count = m.str_count + m.fstr_count;

    printf("  Measured: %zu entries, %zu str + %zu fstr, pool=%zu B\n",
           m.entry_count, m.str_count, m.fstr_count, m.str_pool_size);

    free_buffers();

    entries = malloc(m.entry_count * sizeof(cfgpack_entry_t));
    values = malloc(m.entry_count * sizeof(cfgpack_value_t));
    if (m.str_pool_size > 0) {
        str_pool = malloc(m.str_pool_size);
    }
    if (str_off_count > 0) {
        str_offsets = malloc(str_off_count * sizeof(uint16_t));
    }

    if (!entries || !values || (m.str_pool_size > 0 && !str_pool) ||
        (str_off_count > 0 && !str_offsets)) {
        fprintf(stderr, "malloc failed\n");
        free_buffers();
        return (-1);
    }

    memset(&opts, 0, sizeof(opts));
    opts.out_schema = schema;
    opts.entries = entries;
    opts.max_entries = m.entry_count;
    opts.values = values;
    opts.str_pool = str_pool;
    opts.str_pool_cap = m.str_pool_size > 0 ? m.str_pool_size : 0;
    opts.str_offsets = str_offsets;
    opts.str_offsets_count = str_off_count;
    opts.err = &perr;

    rc = cfgpack_schema_parse_msgpack(mp_data, mp_len, &opts);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Parse error (%s): %s\n", label, perr.message);
        free_buffers();
        return (-1);
    }

    rc = cfgpack_init(ctx, schema, values, m.entry_count, str_pool,
                      m.str_pool_size > 0 ? m.str_pool_size : 0, str_offsets,
                      str_off_count);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Init failed: %d\n", rc);
        free_buffers();
        return (-1);
    }

    *out_m = m;
    return (0);
}

/**
 * Read raw bytes from a LittleFS file into the scratch buffer.
 *
 * Replicates the scratch-split from cfgpack_pageout_lfs/cfgpack_pagein_lfs:
 * first cache_size bytes = LFS file cache, remainder = data.
 *
 * Use this when you need the raw msgpack blob for cfgpack_peek_name or
 * cfgpack_pagein_remap instead of the convenience cfgpack_pagein_lfs.
 */
static int read_lfs_raw(lfs_t *fs,
                        const char *path,
                        uint8_t *scratch,
                        size_t scratch_cap,
                        uint8_t **out_data,
                        size_t *out_len) {
    lfs_size_t cache_size = fs->cfg->cache_size;
    struct lfs_file_config file_cfg;
    uint8_t *file_cache;
    uint8_t *data_buf;
    lfs_ssize_t fsize;
    size_t data_cap;
    lfs_ssize_t n;
    lfs_file_t f;

    if (scratch_cap <= cache_size) {
        fprintf(stderr, "Scratch too small for LFS cache\n");
        return (-1);
    }

    file_cache = scratch;
    data_buf = scratch + cache_size;
    data_cap = scratch_cap - cache_size;

    memset(&file_cfg, 0, sizeof(file_cfg));
    file_cfg.buffer = file_cache;

    if (lfs_file_opencfg(fs, &f, path, LFS_O_RDONLY, &file_cfg) < 0) {
        fprintf(stderr, "Failed to open LFS file: %s\n", path);
        return (-1);
    }

    fsize = lfs_file_size(fs, &f);
    if (fsize < 0 || (size_t)fsize > data_cap) {
        lfs_file_close(fs, &f);
        fprintf(stderr, "LFS file too large for scratch\n");
        return (-1);
    }

    n = lfs_file_read(fs, &f, data_buf, (lfs_size_t)fsize);
    lfs_file_close(fs, &f);

    if (n != fsize) {
        fprintf(stderr, "LFS read error\n");
        return (-1);
    }

    *out_data = data_buf;
    *out_len = (size_t)n;
    return (0);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Check helpers
 * ───────────────────────────────────────────────────────────────────────────── */

static int check_u32(const cfgpack_ctx_t *ctx,
                     uint16_t idx,
                     uint32_t expect,
                     const char *tag) {
    uint32_t v;
    if (cfgpack_get_u32(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  (idx %-3u) expected %u  [%s]\n", idx, expect, tag);
        return (1);
    }
    printf("  OK    (idx %-3u) = %-10u  [%s]\n", idx, v, tag);
    return (0);
}

static int check_u16(const cfgpack_ctx_t *ctx,
                     uint16_t idx,
                     uint16_t expect,
                     const char *tag) {
    uint16_t v;
    if (cfgpack_get_u16(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  (idx %-3u) expected %u  [%s]\n", idx, expect, tag);
        return (1);
    }
    printf("  OK    (idx %-3u) = %-10u  [%s]\n", idx, v, tag);
    return (0);
}

static int check_u8(const cfgpack_ctx_t *ctx,
                    uint16_t idx,
                    uint8_t expect,
                    const char *tag) {
    uint8_t v;
    if (cfgpack_get_u8(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  (idx %-3u) expected %u  [%s]\n", idx, expect, tag);
        return (1);
    }
    printf("  OK    (idx %-3u) = %-10u  [%s]\n", idx, v, tag);
    return (0);
}

static int check_i16(const cfgpack_ctx_t *ctx,
                     uint16_t idx,
                     int16_t expect,
                     const char *tag) {
    int16_t v;
    if (cfgpack_get_i16(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  (idx %-3u) expected %d  [%s]\n", idx, expect, tag);
        return (1);
    }
    printf("  OK    (idx %-3u) = %-10d  [%s]\n", idx, v, tag);
    return (0);
}

static int check_str(const cfgpack_ctx_t *ctx,
                     uint16_t idx,
                     const char *expect,
                     const char *tag) {
    const char *s;
    uint16_t len;
    size_t elen = strlen(expect);
    if (cfgpack_get_str(ctx, idx, &s, &len) != CFGPACK_OK || len != elen ||
        memcmp(s, expect, elen) != 0) {
        printf("  FAIL  (idx %-3u) expected \"%s\"  [%s]\n", idx, expect, tag);
        return (1);
    }
    printf("  OK    (idx %-3u) = \"%.*s\"  [%s]\n", idx, (int)len, s, tag);
    return (0);
}

static int check_fstr(const cfgpack_ctx_t *ctx,
                      uint16_t idx,
                      const char *expect,
                      const char *tag) {
    const char *s;
    uint8_t len;
    size_t elen = strlen(expect);
    if (cfgpack_get_fstr(ctx, idx, &s, &len) != CFGPACK_OK || len != elen ||
        memcmp(s, expect, elen) != 0) {
        printf("  FAIL  (idx %-3u) expected \"%s\"  [%s]\n", idx, expect, tag);
        return (1);
    }
    printf("  OK    (idx %-3u) = \"%.*s\"  [%s]\n", idx, (int)len, s, tag);
    return (0);
}

static int check_absent(const cfgpack_ctx_t *ctx,
                        uint16_t idx,
                        const char *tag) {
    cfgpack_value_t tmp;
    if (cfgpack_get(ctx, idx, &tmp) != CFGPACK_ERR_MISSING) {
        printf("  FAIL  (idx %-3u) expected <absent>  [%s]\n", idx, tag);
        return (1);
    }
    printf("  OK    (idx %-3u) = <absent>    [%s]\n", idx, tag);
    return (0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    cfgpack_schema_t schema;
    cfgpack_ctx_t ctx;
    cfgpack_schema_measure_t m;
    cfgpack_err_t rc;
    int fail = 0;

    printf(
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║  CFGPack Flash Config: LittleFS + LZ4 schemas + v1->v2           ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n"
        "\n");

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 1: Load v1 schema from LZ4-compressed msgpack
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 1: Load v1 schema (LZ4 decompress -> measure -> parse) ─\n"
           "\n");

    size_t v1_len, v2_len;
    size_t v1_clen, v2_clen;
    uint8_t *v1_mp = read_lz4_file("sensor_v1.msgpack.lz4", &v1_len, &v1_clen);
    uint8_t *v2_mp = read_lz4_file("sensor_v2.msgpack.lz4", &v2_len, &v2_clen);
    if (!v1_mp || !v2_mp) {
        free(v1_mp);
        free(v2_mp);
        return (1);
    }

    printf("  Schema files (LZ4-compressed -> decompressed):\n");
    printf("    sensor_v1.msgpack.lz4: %4zu -> %4zu bytes (%.1f%%)\n", v1_clen,
           v1_len, (100.0 * v1_clen / v1_len));
    printf("    sensor_v2.msgpack.lz4: %4zu -> %4zu bytes (%.1f%%)\n", v2_clen,
           v2_len, (100.0 * v2_clen / v2_len));
    printf("\n");

    if (load_msgpack_schema(v1_mp, v1_len, "v1", &schema, &ctx, &m) != 0) {
        return (1);
    }

    printf("  Loaded: %s v%u (%zu entries)\n\n", schema.map_name,
           schema.version, schema.entry_count);

    print_memory_report("v1", &m);

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 2: Configure v1 and write to LittleFS
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 2: Set v1 values, write to LittleFS ───────────────────\n"
           "\n");

    cfgpack_set_u32(&ctx, 1, 1001);          /* devid (KEEP) */
    cfgpack_set_str(&ctx, 2, "press-bay-7"); /* dname (KEEP str) */
    cfgpack_set_fstr(&ctx, 3, "bay-7");      /* loc (KEEP fstr) */
    cfgpack_set_u8(&ctx, 5, 1);              /* tmen (REMOVE) */
    cfgpack_set_u16(&ctx, 6, 2000);          /* tmint (WIDEN u16->u32) */
    cfgpack_set_i16(&ctx, 7, 600);           /* tmhi (MOVE 7->40) */
    cfgpack_set_i16(&ctx, 8, -100);          /* tmlo (MOVE 8->41) */
    cfgpack_set_u8(&ctx, 9, 1);              /* hmen (REMOVE) */
    cfgpack_set_u16(&ctx, 10, 3000);         /* hmint (WIDEN u16->u32) */
    cfgpack_set_u8(&ctx, 11, 85);            /* hmhi (MOVE 11->42) */
    cfgpack_set_u8(&ctx, 12, 25);            /* hmlo (MOVE 12->43) */
    cfgpack_set_u8(&ctx, 13, 0);             /* pren (REMOVE) */
    cfgpack_set_u16(&ctx, 15, 1040);         /* prhi (MOVE 15->44) */
    cfgpack_set_u16(&ctx, 16, 960);          /* prlo (MOVE 16->45) */
    cfgpack_set_str(&ctx, 17, "mqtt.factory.local"); /* srvh (KEEP str) */
    cfgpack_set_u16(&ctx, 18, 1883);                 /* srvp (KEEP) */

    printf("  Set 16 non-default values\n\n");

    /* Format and mount the RAM-backed LittleFS */
    lfs_format(&lfs, &lfs_cfg);
    lfs_mount(&lfs, &lfs_cfg);

    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/config.bin", lfs_scratch,
                             sizeof(lfs_scratch));
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout to LFS failed: %d\n", rc);
        return (1);
    }

    {
        struct lfs_info info;
        lfs_stat(&lfs, "/config.bin", &info);
        printf("  Written to LittleFS: /config.bin (%lu bytes on flash)\n",
               (unsigned long)info.size);
        printf("  Scratch buffer: %d total = %d (LFS cache) + %d (data)\n\n",
               LFS_SCRATCH_SIZE, BLOCK_SIZE, LFS_SCRATCH_SIZE - BLOCK_SIZE);
    }

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 3: Firmware upgrade — detect version, load v2, migrate
     *
     * cfgpack_pagein_lfs() wraps cfgpack_pagein_buf() only, so migration
     * requires reading the raw blob from LFS and calling pagein_remap().
     * This is the composable I/O pattern: the storage layer (LFS) and
     * the parsing layer (pagein_remap) are intentionally decoupled.
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 3: Detect stored schema, load v2, migrate ─────────────\n"
           "\n");

    /* Read raw config from LFS for peek + remap */
    uint8_t *raw_data;
    size_t raw_len;
    if (read_lfs_raw(&lfs, "/config.bin", lfs_scratch, sizeof(lfs_scratch),
                     &raw_data, &raw_len) != 0) {
        return (1);
    }

    char stored_name[64];
    rc = cfgpack_peek_name(raw_data, raw_len, stored_name, sizeof(stored_name));
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "peek_name failed: %d\n", rc);
        return (1);
    }
    printf("  LFS /config.bin contains: \"%s\" (%zu bytes)\n", stored_name,
           raw_len);

    /* Load v2 schema */
    if (load_msgpack_schema(v2_mp, v2_len, "v2", &schema, &ctx, &m) != 0) {
        return (1);
    }
    printf("  Loaded: %s v%u (%zu entries)\n\n", schema.map_name,
           schema.version, schema.entry_count);

    print_memory_report("v2", &m);

    /* Apply remap migration */
    printf("  Applying v1->v2 remap (%zu MOVE entries):\n", REMAP_COUNT);
    for (size_t i = 0; i < REMAP_COUNT; i++) {
        printf("    old %u -> new %u\n", v1_to_v2_remap[i].old_index,
               v1_to_v2_remap[i].new_index);
    }
    printf("\n");

    rc = cfgpack_pagein_remap(&ctx, raw_data, raw_len, v1_to_v2_remap,
                              REMAP_COUNT);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "pagein_remap failed: %d\n", rc);
        return (1);
    }

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 4: Verify v1 -> v2 migration
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 4: Verify migration ───────────────────────────────────\n"
           "\n");

    /* KEEP: values at same indices preserved */
    fail += check_u32(&ctx, 1, 1001, "KEEP devid");
    fail += check_str(&ctx, 2, "press-bay-7", "KEEP dname");
    fail += check_fstr(&ctx, 3, "bay-7", "KEEP loc");
    fail += check_fstr(&ctx, 4, "1.0.0", "KEEP fwver (v1 default)");
    fail += check_str(&ctx, 17, "mqtt.factory.local", "KEEP srvh");
    fail += check_u16(&ctx, 18, 1883, "KEEP srvp");

    /* WIDEN: u16 -> u32 (value preserved, type widened) */
    fail += check_u32(&ctx, 6, 2000, "WIDEN tmint u16->u32");
    fail += check_u32(&ctx, 10, 3000, "WIDEN hmint u16->u32");

    /* MOVE: thresholds relocated to alarm block */
    fail += check_i16(&ctx, 40, 600, "MOVE tmhi 7->40");
    fail += check_i16(&ctx, 41, -100, "MOVE tmlo 8->41");
    fail += check_u8(&ctx, 42, 85, "MOVE hmhi 11->42");
    fail += check_u8(&ctx, 43, 25, "MOVE hmlo 12->43");
    fail += check_u16(&ctx, 44, 1040, "MOVE prhi 15->44");
    fail += check_u16(&ctx, 45, 960, "MOVE prlo 16->45");

    /* REMOVE: per-sensor enables absent in v2 */
    fail += check_absent(&ctx, 5, "REMOVE tmen");
    fail += check_absent(&ctx, 9, "REMOVE hmen");
    fail += check_absent(&ctx, 13, "REMOVE pren");

    /* ADD: new entries get v2 defaults */
    fail += check_u8(&ctx, 30, 7, "ADD smask default");
    fail += check_u8(&ctx, 31, 2, "ADD loglv default");
    fail += check_str(&ctx, 32, "pool.ntp.org", "ADD ntph default");
    fail += check_fstr(&ctx, 33, "UTC+0", "ADD tzone default");
    fail += check_u16(&ctx, 34, 10, "ADD batch default");

    printf("\n");

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 5: Write migrated v2 config back to LittleFS
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 5: Update firmware version, write v2 to LittleFS ──────\n"
           "\n");

    cfgpack_set_fstr(&ctx, 4, "2.0.0");

    rc = cfgpack_pageout_lfs(&ctx, &lfs, "/config.bin", lfs_scratch,
                             sizeof(lfs_scratch));
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout v2 failed: %d\n", rc);
        return (1);
    }
    printf("  Serialized %zu entries to LittleFS\n\n", cfgpack_get_size(&ctx));

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 6: Same-version reload via convenience wrapper
     *
     * Contrast with Phase 3: same-version reload uses cfgpack_pagein_lfs()
     * directly. Cross-version migration required manual read + remap.
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 6: Reload v2 from LittleFS (convenience wrapper) ──────\n"
           "\n");

    memset(values, 0, m.entry_count * sizeof(cfgpack_value_t));
    memset(ctx.present, 0, sizeof(ctx.present));

    rc = cfgpack_pagein_lfs(&ctx, &lfs, "/config.bin", lfs_scratch,
                            sizeof(lfs_scratch));
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pagein v2 from LFS failed: %d\n", rc);
        return (1);
    }

    fail += check_u32(&ctx, 1, 1001, "RELOAD devid");
    fail += check_fstr(&ctx, 4, "2.0.0", "RELOAD fwver");
    fail += check_u32(&ctx, 6, 2000, "RELOAD tmint");
    fail += check_i16(&ctx, 40, 600, "RELOAD tmhi");
    fail += check_u8(&ctx, 30, 7, "RELOAD smask");
    fail += check_str(&ctx, 32, "pool.ntp.org", "RELOAD ntph");

    printf("\n");

    lfs_unmount(&lfs);

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 7: Full dump and summary
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 7: Full v2 configuration ──────────────────────────────\n"
           "\n");
    dump_entries(&ctx);

    printf("── Summary ─────────────────────────────────────────────────────\n"
           "\n");

    {
        size_t alloc_total = m.entry_count * sizeof(cfgpack_entry_t) +
                             m.entry_count * sizeof(cfgpack_value_t) +
                             m.str_pool_size +
                             (m.str_count + m.fstr_count) * sizeof(uint16_t);

        printf("  Schema format:          LZ4-compressed msgpack binary\n");
        printf("  Storage backend:        LittleFS (RAM-backed %d x %d = "
               "%d B)\n",
               BLOCK_COUNT, BLOCK_SIZE, BLOCK_COUNT * BLOCK_SIZE);
        printf("  Migration:              sensor_v1 -> sensor_v2\n");
        printf("  LFS scratch buffer:     %d bytes (%d cache + %d data)\n",
               LFS_SCRATCH_SIZE, BLOCK_SIZE, LFS_SCRATCH_SIZE - BLOCK_SIZE);
        printf("  v2 entries (measured):  %zu\n", m.entry_count);
        printf("  v2 strings (measured):  %zu str + %zu fstr\n", m.str_count,
               m.fstr_count);
        printf("  Heap allocated:         %zu bytes (from measure)\n",
               alloc_total);
        printf("  Fixed overhead:         %zu bytes (ctx + schema on stack)\n",
               sizeof(cfgpack_ctx_t) + sizeof(cfgpack_schema_t));
        printf("  Entries present:        %zu\n", cfgpack_get_size(&ctx));
    }

    if (fail > 0) {
        printf("\n  FAILED: %d check(s) did not pass.\n", fail);
    } else {
        printf("\n  All checks passed.\n");
    }

    free_buffers();
    free(v1_mp);
    free(v2_mp);
    return (fail > 0 ? 1 : 0);
}
