/**
 * @file main.c
 * @brief CFGPack Fleet Gateway Example — msgpack binary schemas + migration
 *
 * Demonstrates:
 * - Loading schemas from MessagePack binary format (built by cfgpack-schema-pack)
 * - Using cfgpack_schema_measure_msgpack() for minimal-stack buffer sizing
 * - Heap allocation of right-sized buffers (no static arrays)
 * - Three-version migration chain: v1 -> v2 -> v3
 * - All five migration scenarios: KEEP, WIDEN, MOVE, REMOVE, ADD
 *
 * Scenario: Fleet management gateway firmware upgrade.  v1 tracks basic
 * vehicle identity, GPS, network, and alerts (32 entries, 16 strings).
 * v2 adds geofencing, MQTT topics, driver info, widens GPS/OBD intervals,
 * and moves alert thresholds (36 entries, 18 strings).  v3 adds OTA,
 * telemetry, trip tracking, widens timeouts/radii, and removes per-zone
 * enables in favor of a bitmask (38 entries, 20 strings).
 *
 * Build pipeline (handled by Makefile):
 *   .map  --(cfgpack-schema-pack)-->  .msgpack  --(this program)-->  runtime
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfgpack/cfgpack.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Remap tables
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * v1 -> v2 remap: alert thresholds moved from indices 20-23 to 60-63.
 *
 * Implicit migrations (no remap entry needed):
 *   KEEP:   indices 1-10, 12-19, 25-32 exist in both schemas
 *   WIDEN:  idx 9 (gpsrt u16->u32), idx 25 (obdrt u16->u32)
 *   REMOVE: idx 11 (gpsmd), idx 24 (dtcen) absent in v2
 *   ADD:    idx 40-52 (driver info, geofences, MQTT topics)
 */
static const cfgpack_remap_entry_t v1_to_v2_remap[] = {
    {20, 60}, /* aspd:  speed alert      -> moved to index 60 */
    {21, 61}, /* aidle: idle alert       -> moved to index 61 */
    {22, 62}, /* atmp:  low temp alert   -> moved to index 62 */
    {23, 63}, /* afuel: low fuel alert   -> moved to index 63 */
};
#define V1_V2_REMAP_COUNT (sizeof(v1_to_v2_remap) / sizeof(v1_to_v2_remap[0]))

/**
 * v2 -> v3 remap: no index moves needed.
 *
 * Implicit migrations:
 *   KEEP:   most indices preserved
 *   WIDEN:  idx 17 (tout u16->u32), idx 44 (gf0rd u16->u32),
 *           idx 47 (gf1rd u16->u32)
 *   REMOVE: idx 43 (gf0en), idx 46 (gf1en) merged into gfflg
 *   ADD:    idx 70-78 (geofence flags, OTA, telemetry, trip tracking)
 */
static const cfgpack_remap_entry_t *v2_to_v3_remap = NULL;
#define V2_V3_REMAP_COUNT 0

/* ═══════════════════════════════════════════════════════════════════════════
 * Simulated flash storage for serialized config
 * ═══════════════════════════════════════════════════════════════════════════ */
static uint8_t flash[2048];
static size_t flash_len;

/* ═══════════════════════════════════════════════════════════════════════════
 * Currently active heap-allocated buffers
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

/**
 * Print a compact table of all present entries.
 */
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

/**
 * Print memory accounting for measured + allocated buffers.
 */
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

/**
 * Free previously allocated schema buffers.
 */
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

/**
 * Load a msgpack binary schema using the two-phase pattern:
 *   1. cfgpack_schema_measure_msgpack() — scan binary, count entries/strings
 *   2. malloc right-sized buffers
 *   3. cfgpack_schema_parse_msgpack() — parse into those buffers
 *   4. cfgpack_init() — wire buffers into context
 *
 * The measure call uses ~288 bytes of stack (O0) and requires no output
 * buffers — ideal for constrained targets.
 */
static int load_msgpack_schema(const uint8_t *mp_data,
                               size_t mp_len,
                               const char *label,
                               cfgpack_schema_t *schema,
                               cfgpack_ctx_t *ctx,
                               cfgpack_schema_measure_t *out_m) {
    cfgpack_err_t rc;
    cfgpack_parse_error_t perr;

    /* Phase 1: Measure — discover entry count and string sizing */
    cfgpack_schema_measure_t m;
    rc = cfgpack_schema_measure_msgpack(mp_data, mp_len, &m, &perr);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Measure error (%s): %s\n", label, perr.message);
        return -1;
    }

    if (m.entry_count > CFGPACK_MAX_ENTRIES) {
        fprintf(stderr,
                "Schema has %zu entries, exceeds CFGPACK_MAX_ENTRIES=%d\n",
                m.entry_count, CFGPACK_MAX_ENTRIES);
        return -1;
    }

    size_t str_off_count = m.str_count + m.fstr_count;

    printf("  Measured: %zu entries, %zu str + %zu fstr, pool=%zu B\n",
           m.entry_count, m.str_count, m.fstr_count, m.str_pool_size);

    /* Phase 2: Allocate right-sized buffers */
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
        return -1;
    }

    /* Phase 3: Parse msgpack binary into measured buffers */
    cfgpack_parse_opts_t opts = {
        .out_schema = schema,
        .entries = entries,
        .max_entries = m.entry_count,
        .values = values,
        .str_pool = str_pool,
        .str_pool_cap = m.str_pool_size > 0 ? m.str_pool_size : 0,
        .str_offsets = str_offsets,
        .str_offsets_count = str_off_count,
        .err = &perr,
    };

    rc = cfgpack_schema_parse_msgpack(mp_data, mp_len, &opts);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Parse error (%s): %s\n", label, perr.message);
        free_buffers();
        return -1;
    }

    /* Phase 4: Initialize context */
    rc = cfgpack_init(ctx, schema, values, m.entry_count, str_pool,
                      m.str_pool_size > 0 ? m.str_pool_size : 0, str_offsets,
                      str_off_count);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Init failed: %d\n", rc);
        free_buffers();
        return -1;
    }

    *out_m = m;
    return 0;
}

/**
 * Read a binary file into a malloc'd buffer.
 */
static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        fprintf(stderr, "Empty or unreadable: %s\n", path);
        return NULL;
    }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "malloc failed for %s\n", path);
        return NULL;
    }
    *out_len = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    return buf;
}

/**
 * Check a string value matches expected. Returns 0 on match, 1 on mismatch.
 */
static int check_str(const cfgpack_ctx_t *ctx,
                     uint16_t idx,
                     const char *expect,
                     const char *tag) {
    const char *s;
    uint16_t len;
    size_t elen = strlen(expect);
    if (cfgpack_get_str(ctx, idx, &s, &len) != CFGPACK_OK || len != elen ||
        memcmp(s, expect, elen) != 0) {
        printf("  FAIL  %-5s (idx %-3u) = ???  (expected \"%s\") [%s]\n", "",
               idx, expect, tag);
        return 1;
    }
    printf("  OK    %-5s (idx %-3u) = \"%.*s\"  [%s]\n", "", idx, (int)len, s,
           tag);
    return 0;
}

/**
 * Check an fstr value matches expected. Returns 0 on match, 1 on mismatch.
 */
static int check_fstr(const cfgpack_ctx_t *ctx,
                      uint16_t idx,
                      const char *expect,
                      const char *tag) {
    const char *s;
    uint8_t len;
    size_t elen = strlen(expect);
    if (cfgpack_get_fstr(ctx, idx, &s, &len) != CFGPACK_OK || len != elen ||
        memcmp(s, expect, elen) != 0) {
        printf("  FAIL  %-5s (idx %-3u) = ???  (expected \"%s\") [%s]\n", "",
               idx, expect, tag);
        return 1;
    }
    printf("  OK    %-5s (idx %-3u) = \"%.*s\"  [%s]\n", "", idx, (int)len, s,
           tag);
    return 0;
}

/**
 * Check a uint32 value matches expected.
 */
static int check_u32(const cfgpack_ctx_t *ctx,
                     uint16_t idx,
                     uint32_t expect,
                     const char *tag) {
    uint32_t v;
    if (cfgpack_get_u32(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  %-5s (idx %-3u) = ???  (expected %u) [%s]\n", "", idx,
               expect, tag);
        return 1;
    }
    printf("  OK    %-5s (idx %-3u) = %u  [%s]\n", "", idx, v, tag);
    return 0;
}

/**
 * Check a uint16 value matches expected.
 */
static int check_u16(const cfgpack_ctx_t *ctx,
                     uint16_t idx,
                     uint16_t expect,
                     const char *tag) {
    uint16_t v;
    if (cfgpack_get_u16(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  %-5s (idx %-3u) = ???  (expected %u) [%s]\n", "", idx,
               expect, tag);
        return 1;
    }
    printf("  OK    %-5s (idx %-3u) = %u  [%s]\n", "", idx, v, tag);
    return 0;
}

/**
 * Check a uint8 value matches expected.
 */
static int check_u8(const cfgpack_ctx_t *ctx,
                    uint16_t idx,
                    uint8_t expect,
                    const char *tag) {
    uint8_t v;
    if (cfgpack_get_u8(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  %-5s (idx %-3u) = ???  (expected %u) [%s]\n", "", idx,
               expect, tag);
        return 1;
    }
    printf("  OK    %-5s (idx %-3u) = %u  [%s]\n", "", idx, v, tag);
    return 0;
}

/**
 * Check an i8 value matches expected.
 */
static int check_i8(const cfgpack_ctx_t *ctx,
                    uint16_t idx,
                    int8_t expect,
                    const char *tag) {
    int8_t v;
    if (cfgpack_get_i8(ctx, idx, &v) != CFGPACK_OK || v != expect) {
        printf("  FAIL  %-5s (idx %-3u) = ???  (expected %d) [%s]\n", "", idx,
               expect, tag);
        return 1;
    }
    printf("  OK    %-5s (idx %-3u) = %d  [%s]\n", "", idx, v, tag);
    return 0;
}

/**
 * Check that an index is absent from the schema.
 */
static int check_absent(const cfgpack_ctx_t *ctx,
                        uint16_t idx,
                        const char *tag) {
    cfgpack_value_t tmp;
    cfgpack_err_t rc = cfgpack_get(ctx, idx, &tmp);
    if (rc != CFGPACK_ERR_MISSING) {
        printf("  FAIL  %-5s (idx %-3u) = <present>  (expected absent) [%s]\n",
               "", idx, tag);
        return 1;
    }
    printf("  OK    %-5s (idx %-3u) = <absent>  [%s]\n", "", idx, tag);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    cfgpack_err_t rc;
    cfgpack_schema_t schema;
    cfgpack_ctx_t ctx;
    cfgpack_schema_measure_t m;
    int fail = 0;

    printf(
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║  CFGPack Fleet Gateway: msgpack binary schemas + v1->v2->v3      ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n"
        "\n");

    /* ── Load schema files from disk ──────────────────────────────────────
     * These are pre-built by `make` using cfgpack-schema-pack.
     * On a real device, the binary schema would be embedded in firmware
     * or received via OTA — no text parsing needed at runtime. */

    size_t v1_len, v2_len, v3_len;
    uint8_t *v1_mp = read_file("fleet_v1.msgpack", &v1_len);
    uint8_t *v2_mp = read_file("fleet_v2.msgpack", &v2_len);
    uint8_t *v3_mp = read_file("fleet_v3.msgpack", &v3_len);
    if (!v1_mp || !v2_mp || !v3_mp) {
        free(v1_mp);
        free(v2_mp);
        free(v3_mp);
        return 1;
    }

    printf("Schema binary sizes:\n");
    printf("  fleet_v1.msgpack: %4zu bytes\n", v1_len);
    printf("  fleet_v2.msgpack: %4zu bytes\n", v2_len);
    printf("  fleet_v3.msgpack: %4zu bytes\n", v3_len);
    printf("\n");

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 1: Boot with v1 schema
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 1: Load v1 schema (measure_msgpack -> malloc -> parse) ─\n"
           "\n");

    if (load_msgpack_schema(v1_mp, v1_len, "v1", &schema, &ctx, &m) != 0) {
        return 1;
    }

    printf("  Loaded: %s v%u (%zu entries)\n\n", schema.map_name,
           schema.version, schema.entry_count);

    print_memory_report("v1", &m);

    /* Set non-default values that will be migrated */
    cfgpack_set_u32(&ctx, 1, 42);              /* vid = 42 */
    cfgpack_set_str(&ctx, 2, "big-rig-07");    /* vname */
    cfgpack_set_fstr(&ctx, 3, "heavy");        /* vtype */
    cfgpack_set_str(&ctx, 5, "alice");         /* drv */
    cfgpack_set_str(&ctx, 6, "DRV-1234");      /* drvid */
    cfgpack_set_fstr(&ctx, 7, "north");        /* fleet */
    cfgpack_set_str(&ctx, 8, "depot-b");       /* depot */
    cfgpack_set_u16(&ctx, 9, 2000);            /* gpsrt (will widen) */
    cfgpack_set_u8(&ctx, 11, 1);               /* gpsmd (will be removed) */
    cfgpack_set_str(&ctx, 13, "gps.fleet.io"); /* srvh */
    cfgpack_set_str(&ctx, 15, "key-abc-123");  /* apik */
    cfgpack_set_fstr(&ctx, 19, "http");        /* proto */
    cfgpack_set_u16(&ctx, 20, 130);            /* aspd (will move) */
    cfgpack_set_u16(&ctx, 21, 600);            /* aidle (will move) */
    cfgpack_set_i8(&ctx, 22, -30);             /* atmp (will move) */
    cfgpack_set_u8(&ctx, 23, 10);              /* afuel (will move) */
    cfgpack_set_u8(&ctx, 24, 0);               /* dtcen (will be removed) */
    cfgpack_set_u16(&ctx, 25, 5000);           /* obdrt (will widen) */
    cfgpack_set_str(&ctx, 26, "fleet.log");    /* logfn */
    cfgpack_set_fstr(&ctx, 28, "gw-007");      /* dname */
    cfgpack_set_str(&ctx, 29, "SN-2024-ABC");  /* dser */
    cfgpack_set_str(&ctx, 32, "acme-fleet");   /* mfg */

    printf("  Set 21 non-default values (mix of numeric and string)\n\n");

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 2: Serialize v1 config to flash
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 2: Serialize v1 to flash ───────────────────────────────\n"
           "\n");

    rc = cfgpack_pageout(&ctx, flash, sizeof(flash), &flash_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout failed: %d\n", rc);
        return 1;
    }
    printf("  Serialized %zu entries -> %zu bytes of MessagePack\n\n",
           cfgpack_get_size(&ctx), flash_len);

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 3: Firmware upgrade v1 -> v2
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 3: Detect stored schema, load v2, migrate ──────────────\n"
           "\n");

    char stored_name[64];
    rc = cfgpack_peek_name(flash, flash_len, stored_name, sizeof(stored_name));
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "peek_name failed: %d\n", rc);
        return 1;
    }
    printf("  Flash contains: \"%s\"\n", stored_name);

    if (load_msgpack_schema(v2_mp, v2_len, "v2", &schema, &ctx, &m) != 0) {
        return 1;
    }
    printf("  Loaded: %s v%u (%zu entries)\n\n", schema.map_name,
           schema.version, schema.entry_count);

    print_memory_report("v2", &m);

    /* Apply v1->v2 migration */
    printf("  Applying v1->v2 remap (%zu entries):\n", V1_V2_REMAP_COUNT);
    for (size_t i = 0; i < V1_V2_REMAP_COUNT; i++) {
        printf("    old %u -> new %u\n", v1_to_v2_remap[i].old_index,
               v1_to_v2_remap[i].new_index);
    }
    printf("\n");

    rc = cfgpack_pagein_remap(&ctx, flash, flash_len, v1_to_v2_remap,
                              V1_V2_REMAP_COUNT);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "v1->v2 pagein_remap failed: %d\n", rc);
        return 1;
    }

    /* ── Verify v1 -> v2 migration ──────────────────────────────────────── */

    printf("── Phase 3a: Verify v1 -> v2 migration ─────────────────────────\n"
           "\n");

    /* KEEP: strings and numerics at same indices */
    fail += check_u32(&ctx, 1, 42, "KEEP vid");
    fail += check_str(&ctx, 2, "big-rig-07", "KEEP vname");
    fail += check_fstr(&ctx, 3, "heavy", "KEEP vtype");
    fail += check_str(&ctx, 5, "alice", "KEEP drv");
    fail += check_str(&ctx, 6, "DRV-1234", "KEEP drvid");
    fail += check_fstr(&ctx, 7, "north", "KEEP fleet");
    fail += check_str(&ctx, 8, "depot-b", "KEEP depot");
    fail += check_str(&ctx, 13, "gps.fleet.io", "KEEP srvh");
    fail += check_str(&ctx, 15, "key-abc-123", "KEEP apik");
    fail += check_fstr(&ctx, 19, "http", "KEEP proto");
    fail += check_str(&ctx, 26, "fleet.log", "KEEP logfn");
    fail += check_fstr(&ctx, 28, "gw-007", "KEEP dname");
    fail += check_str(&ctx, 29, "SN-2024-ABC", "KEEP dser");
    fail += check_str(&ctx, 32, "acme-fleet", "KEEP mfg");

    /* WIDEN: u16 -> u32 */
    fail += check_u32(&ctx, 9, 2000, "WIDEN gpsrt u16->u32");
    fail += check_u32(&ctx, 25, 5000, "WIDEN obdrt u16->u32");

    /* MOVE: alert thresholds 20-23 -> 60-63 */
    fail += check_u16(&ctx, 60, 130, "MOVE aspd 20->60");
    fail += check_u16(&ctx, 61, 600, "MOVE aidle 21->61");
    fail += check_i8(&ctx, 62, -30, "MOVE atmp 22->62");
    fail += check_u8(&ctx, 63, 10, "MOVE afuel 23->63");

    /* REMOVE: indices 11 and 24 absent in v2 */
    fail += check_absent(&ctx, 11, "REMOVE gpsmd");
    fail += check_absent(&ctx, 24, "REMOVE dtcen");

    /* ADD: new v2 entries should have defaults */
    fail += check_str(&ctx, 42, "warehouse", "ADD gf0nm default");
    fail += check_u8(&ctx, 43, 1, "ADD gf0en default");
    fail += check_u16(&ctx, 44, 500, "ADD gf0rd default");
    fail += check_str(&ctx, 45, "hq-campus", "ADD gf1nm default");
    fail += check_fstr(&ctx, 50, "pos", "ADD mtpos default");
    fail += check_fstr(&ctx, 51, "alert", "ADD mtalr default");
    fail += check_fstr(&ctx, 52, "diag", "ADD mtdg default");

    /* fwver (idx 30) exists in both v1 and v2 — it's KEEP, not ADD.
     * The old "1.0.0" value is preserved.  Real firmware would update it. */
    fail += check_fstr(&ctx, 30, "1.0.0", "KEEP fwver (from v1)");

    printf("\n");

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 4: Modify some v2 values, serialize to flash
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 4: Modify v2 config, serialize to flash ────────────────\n"
           "\n");

    /* Update fwver after migration (real firmware would do this on boot) */
    cfgpack_set_fstr(&ctx, 30, "2.0.0"); /* fwver = 2.0.0 */

    cfgpack_set_str(&ctx, 5, "bob");          /* drv = bob */
    cfgpack_set_str(&ctx, 40, "+1-555-0199"); /* dph (new in v2) */
    cfgpack_set_fstr(&ctx, 41, "CDL-A");      /* dlic (new in v2) */
    cfgpack_set_str(&ctx, 42, "depot-north"); /* gf0nm */
    cfgpack_set_u16(&ctx, 44, 750);           /* gf0rd */
    cfgpack_set_u8(&ctx, 46, 1);      /* gf1en = 1 (will be removed in v3) */
    cfgpack_set_u16(&ctx, 47, 2000);  /* gf1rd (will widen) */
    cfgpack_set_u16(&ctx, 17, 60000); /* tout (will widen) */

    printf("  Modified 9 values for v2\n\n");

    rc = cfgpack_pageout(&ctx, flash, sizeof(flash), &flash_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout v2 failed: %d\n", rc);
        return 1;
    }
    printf("  Serialized %zu entries -> %zu bytes\n\n", cfgpack_get_size(&ctx),
           flash_len);

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 5: Firmware upgrade v2 -> v3
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 5: Detect stored schema, load v3, migrate ──────────────\n"
           "\n");

    rc = cfgpack_peek_name(flash, flash_len, stored_name, sizeof(stored_name));
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "peek_name failed: %d\n", rc);
        return 1;
    }
    printf("  Flash contains: \"%s\"\n", stored_name);

    if (load_msgpack_schema(v3_mp, v3_len, "v3", &schema, &ctx, &m) != 0) {
        return 1;
    }
    printf("  Loaded: %s v%u (%zu entries)\n\n", schema.map_name,
           schema.version, schema.entry_count);

    print_memory_report("v3", &m);

    /* v2->v3 has no index moves, only widen/remove/add */
    printf("  v2->v3 remap: no index moves (widen + remove + add only)\n\n");

    rc = cfgpack_pagein_remap(&ctx, flash, flash_len, v2_to_v3_remap,
                              V2_V3_REMAP_COUNT);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "v2->v3 pagein_remap failed: %d\n", rc);
        return 1;
    }

    /* ── Verify v2 -> v3 migration ──────────────────────────────────────── */

    printf("── Phase 5a: Verify v2 -> v3 migration ─────────────────────────\n"
           "\n");

    /* KEEP: values carried from v1->v2->v3 */
    fail += check_u32(&ctx, 1, 42, "KEEP vid (from v1)");
    fail += check_str(&ctx, 2, "big-rig-07", "KEEP vname (from v1)");
    fail += check_str(&ctx, 5, "bob", "KEEP drv (set in v2)");
    fail += check_str(&ctx, 6, "DRV-1234", "KEEP drvid (from v1)");
    fail += check_fstr(&ctx, 7, "north", "KEEP fleet (from v1)");
    fail += check_str(&ctx, 13, "gps.fleet.io", "KEEP srvh (from v1)");
    fail += check_str(&ctx, 15, "key-abc-123", "KEEP apik (from v1)");
    fail += check_str(&ctx, 40, "+1-555-0199", "KEEP dph (set in v2)");
    fail += check_fstr(&ctx, 41, "CDL-A", "KEEP dlic (set in v2)");
    fail += check_str(&ctx, 42, "depot-north", "KEEP gf0nm (set in v2)");
    fail += check_fstr(&ctx, 50, "pos", "KEEP mtpos");
    fail += check_fstr(&ctx, 51, "alert", "KEEP mtalr");

    /* WIDEN: u16 -> u32 in v3 */
    fail += check_u32(&ctx, 17, 60000, "WIDEN tout u16->u32");
    fail += check_u32(&ctx, 44, 750, "WIDEN gf0rd u16->u32");
    fail += check_u32(&ctx, 47, 2000, "WIDEN gf1rd u16->u32");

    /* REMOVE: geofence enables merged into gfflg */
    fail += check_absent(&ctx, 43, "REMOVE gf0en");
    fail += check_absent(&ctx, 46, "REMOVE gf1en");

    /* ADD: new v3 entries should have defaults */
    fail += check_u8(&ctx, 70, 3, "ADD gfflg default");
    fail += check_u8(&ctx, 71, 1, "ADD otaen default");
    fail += check_str(&ctx, 72, "ota.example.com/fw", "ADD otaur default");
    fail += check_u32(&ctx, 73, 0, "ADD otacr default");
    fail += check_u16(&ctx, 74, 3600, "ADD otaiv default");
    fail += check_str(&ctx, 75, "telemetry.example.com", "ADD telep default");
    fail += check_fstr(&ctx, 76, "proto", "ADD telfm default");
    fail += check_u8(&ctx, 78, 1, "ADD trpen default");

    /* fwver and hwver exist in all schemas — they're KEEP, not ADD.
     * fwver was explicitly set to "2.0.0" in Phase 4, hwver is still "rev-a". */
    fail += check_fstr(&ctx, 30, "2.0.0", "KEEP fwver (set in v2)");
    fail += check_fstr(&ctx, 31, "rev-a", "KEEP hwver (from v1)");

    printf("\n");

    /* ═════════════════════════════════════════════════════════════════════
     * Phase 6: Full dump of final v3 config
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Phase 6: Full v3 configuration after migration chain ─────────\n"
           "\n");
    dump_entries(&ctx);

    /* ═════════════════════════════════════════════════════════════════════
     * Summary
     * ═════════════════════════════════════════════════════════════════════ */

    printf("── Summary ──────────────────────────────────────────────────────\n"
           "\n");

    size_t alloc_total = m.entry_count * sizeof(cfgpack_entry_t) +
                         m.entry_count * sizeof(cfgpack_value_t) +
                         m.str_pool_size +
                         (m.str_count + m.fstr_count) * sizeof(uint16_t);

    printf(
        "  Schema format:          msgpack binary (pre-compiled from .map)\n");
    printf("  Migration chain:        fleet_v1 -> fleet_v2 -> fleet_v3\n");
    printf("  v3 entries (measured):  %zu\n", m.entry_count);
    printf("  v3 strings (measured):  %zu str + %zu fstr = %zu total\n",
           m.str_count, m.fstr_count, m.str_count + m.fstr_count);
    printf("  Heap allocated:         %zu bytes (from measure)\n", alloc_total);
    printf("  Fixed overhead:         %zu bytes (ctx + schema on stack)\n",
           sizeof(cfgpack_ctx_t) + sizeof(cfgpack_schema_t));
    printf("  Flash storage:          %zu bytes\n", flash_len);
    printf("  Entries present:        %zu\n", cfgpack_get_size(&ctx));

    if (fail > 0) {
        printf("\n  FAILED: %d check(s) did not pass.\n", fail);
    } else {
        printf("\n  Migration chain complete. All checks passed.\n");
    }

    /* Cleanup */
    free_buffers();
    free(v1_mp);
    free(v2_mp);
    free(v3_mp);
    return fail > 0 ? 1 : 0;
}
