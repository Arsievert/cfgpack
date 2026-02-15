/**
 * @file main.c
 * @brief CFGPack Low-Memory Example with Schema Migration
 *
 * Demonstrates:
 * - Using cfgpack_schema_measure() to discover buffer sizes at runtime
 * - Allocating exactly what's needed via malloc (no oversized static arrays)
 * - CFGPACK_MAX_ENTRIES set to the library default (128) via config.h
 * - Schema migration from v1 -> v2 using cfgpack_pagein_remap()
 *   covering all five migration scenarios:
 *     1. KEEP   — entries at the same index
 *     2. MOVE   — entries relocated to a new index
 *     3. WIDEN  — type widened (u8 -> u16)
 *     4. REMOVE — old entries dropped (silently ignored)
 *     5. ADD    — new entries with defaults (including str/fstr)
 *
 * Scenario: HVAC zone controller firmware upgrade. v1 manages 12 zones
 * with integer-degree setpoints and 3 fstr identity fields.  v2 drops 2
 * zones, adds humidity/eco features, zone names (str), firmware version
 * (fstr), NTP host (str), contact email (str), and per-zone deadband.
 *
 * v1: 84 entries (3 fstr, 0 str)
 * v2: 94 entries (4 fstr, 7 str)
 *
 * The measure API ensures we allocate the exact buffer sizes for whichever
 * schema we're loading — no compile-time guessing of entry counts or
 * string pool sizes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfgpack/cfgpack.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * v1 -> v2 remap table
 *
 * This table maps OLD indices to NEW indices for entries that moved.
 * Entries not listed are looked up by their original index in the new
 * schema — if found, they're loaded (with optional type widening); if
 * not found, they're silently dropped.
 *
 * Migration scenarios in this table:
 *   MOVE:  old alarm thresholds 75-78 relocated to 92-95
 * Implicit (no remap entry needed):
 *   KEEP:  same index in both schemas (zones 0-9 enables, modes, fans, etc.)
 *   WIDEN: setpoints at indices 13-22 exist in both schemas, u8 -> u16
 *          (type coercion is automatic during pagein_remap)
 *   REMOVE: old indices 11,12,23,24,35,36,47,48,59,60 don't exist in v2
 *           schema, so they are silently ignored
 *   ADD:   new v2 indices 85-108 get their schema defaults
 * ═══════════════════════════════════════════════════════════════════════════ */
static const cfgpack_remap_entry_t v1_to_v2_remap[] = {
    {75, 92}, /* ahi: alarm high temp    -> moved to index 92 */
    {76, 93}, /* alo: alarm low temp     -> moved to index 93 */
    {77, 94}, /* hhi: alarm high humid   -> moved to index 94 */
    {78, 95}, /* hlo: alarm low humid    -> moved to index 95 */
};
#define REMAP_COUNT (sizeof(v1_to_v2_remap) / sizeof(v1_to_v2_remap[0]))

/* ═══════════════════════════════════════════════════════════════════════════
 * Measured schema buffers — allocated at runtime, not compile time.
 *
 * We keep pointers to the currently-active buffers so they can be freed
 * and reallocated when switching schemas.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Schema file buffer (holds .map text during parsing — reclaimable after init) */
static char map_buf[8192];

/* Simulated flash storage for serialized config */
static uint8_t flash[512];
static size_t flash_len;

/* Currently active allocated buffers */
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
    size_t ctx_bytes = sizeof(cfgpack_ctx_t);
    size_t schema_bytes = sizeof(cfgpack_schema_t);

    size_t total = entries_bytes + values_bytes + pool_bytes + offsets_bytes +
                   ctx_bytes + schema_bytes;

    printf("=== Memory Report: %s ===\n", label);
    printf("  cfgpack_ctx_t      %4zu B  (includes %d-byte presence bitmap)\n",
           ctx_bytes, CFGPACK_PRESENCE_BYTES);
    printf("  cfgpack_schema_t   %4zu B\n", schema_bytes);
    printf("  entries[%zu]       %4zu B  (%zu x %zu)\n", m->entry_count,
           entries_bytes, m->entry_count, sizeof(cfgpack_entry_t));
    printf("  values[%zu]        %4zu B  (%zu x %zu)\n", m->entry_count,
           values_bytes, m->entry_count, sizeof(cfgpack_value_t));
    printf("  str_pool           %4zu B  (%zu str x %d + %zu fstr x %d)\n",
           pool_bytes, m->str_count, CFGPACK_STR_MAX + 1, m->fstr_count,
           CFGPACK_FSTR_MAX + 1);
    printf("  str_offsets        %4zu B  (%zu x %zu)\n", offsets_bytes,
           str_off_count, sizeof(uint16_t));
    printf("  ────────────────────────\n");
    printf("  TOTAL              %4zu B  (all allocated from measure)\n\n",
           total);
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
 * Measure a .map file, allocate exact buffers, parse, and init the context.
 *
 * This is the two-phase init pattern:
 *   1. cfgpack_schema_measure() — walk schema text, count entries and strings
 *   2. malloc right-sized buffers based on the measurement
 *   3. cfgpack_parse_schema() — parse into those buffers
 *   4. cfgpack_init() — wire buffers into the context
 *
 * The measurement ensures we never over-allocate or under-allocate.
 * On an embedded target, malloc can be replaced with a bump allocator
 * or pool allocator — the sizes are known before the first allocation.
 */
static int load_schema(const char *path,
                       cfgpack_schema_t *schema,
                       cfgpack_ctx_t *ctx,
                       cfgpack_schema_measure_t *out_m) {
    cfgpack_err_t rc;
    cfgpack_parse_error_t perr;

    /* Read schema file */
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return -1;
    }
    size_t len = fread(map_buf, 1, sizeof(map_buf) - 1, f);
    fclose(f);
    map_buf[len] = '\0';

    /* Phase 1: Measure — discover entry count and string sizing.
     * This walks the schema text without needing any output buffers.
     * Stack cost is ~300 bytes. */
    cfgpack_schema_measure_t m;
    rc = cfgpack_schema_measure(map_buf, len, &m, &perr);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Measure error (%s line %zu): %s\n", path, perr.line,
                perr.message);
        return -1;
    }

    /* Validate against compile-time limit */
    if (m.entry_count > CFGPACK_MAX_ENTRIES) {
        fprintf(stderr,
                "Schema has %zu entries, exceeds CFGPACK_MAX_ENTRIES=%d\n",
                m.entry_count, CFGPACK_MAX_ENTRIES);
        return -1;
    }

    size_t str_off_count = m.str_count + m.fstr_count;

    printf("  Measured: %zu entries, %zu str + %zu fstr, pool=%zu B\n",
           m.entry_count, m.str_count, m.fstr_count, m.str_pool_size);

    /* Phase 2: Allocate right-sized buffers.
     * Free any previous buffers from a prior schema load. */
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

    /* Phase 3: Parse schema into the measured buffers */
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

    rc = cfgpack_parse_schema(map_buf, len, &opts);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Parse error (%s line %zu): %s\n", path, perr.line,
                perr.message);
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    cfgpack_err_t rc;
    cfgpack_schema_t schema;
    cfgpack_ctx_t ctx;
    cfgpack_schema_measure_t m;

    /* ── Phase 1: Boot with v1 schema ────────────────────────────────── */

    printf(
        "╔══════════════════════════════════════════════════════════════╗\n");
    printf(
        "║  CFGPack Low-Memory Example: measure API + schema migration  ║\n");
    printf(
        "╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("── Phase 1: Load v1 schema (measure -> allocate -> parse) ───\n\n");

    if (load_schema("hvac_v1.map", &schema, &ctx, &m) != 0) {
        return 1;
    }

    printf("Loaded: %s v%u (%zu entries)\n\n", schema.map_name, schema.version,
           schema.entry_count);

    print_memory_report("v1 (84 entries, 3 fstr, 0 str)", &m);

    /* Set a few non-default values to verify they survive migration */
    cfgpack_set_u8(&ctx, 1, 1);         /* z0en = 1 (keep) */
    cfgpack_set_u8(&ctx, 13, 25);       /* z0sp = 25 (will widen to u16) */
    cfgpack_set_u8(&ctx, 75, 40);       /* ahi = 40 (will move to idx 92) */
    cfgpack_set_u8(&ctx, 11, 1);        /* zaen = 1 (will be removed) */
    cfgpack_set_fstr(&ctx, 82, "main"); /* dname = "main" (keep) */

    printf("── Modified v1 values ───────────────────────────────────────\n");
    printf("  z0en (idx 1)  = 1   [KEEP: stays at index 1]\n");
    printf("  z0sp (idx 13) = 25  [WIDEN: u8 25 -> u16 25]\n");
    printf("  ahi  (idx 75) = 40  [MOVE: index 75 -> 92]\n");
    printf("  zaen (idx 11) = 1   [REMOVE: zone A dropped in v2]\n");
    printf("  dname(idx 82) = \"main\" [KEEP: fstr preserved]\n\n");

    /* ── Phase 2: Serialize v1 config to flash ───────────────────────── */

    printf("── Phase 2: Serialize v1 to flash ───────────────────────────\n\n");

    rc = cfgpack_pageout(&ctx, flash, sizeof(flash), &flash_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout failed: %d\n", rc);
        return 1;
    }
    printf("  Serialized %zu entries to %zu bytes of MessagePack\n\n",
           cfgpack_get_size(&ctx), flash_len);

    /* ── Phase 3: Firmware upgrade — detect old schema ───────────────── */

    printf("── Phase 3: Detect stored schema version ────────────────────\n\n");

    char stored_name[64];
    rc = cfgpack_peek_name(flash, flash_len, stored_name, sizeof(stored_name));
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "peek_name failed: %d\n", rc);
        return 1;
    }
    printf("  Flash contains schema: \"%s\"\n\n", stored_name);

    /* ── Phase 4: Load v2 schema (re-measure, re-allocate) ────────────
     *
     * This is the key benefit of the measure API: v2 has more entries
     * and more string types than v1, so the buffers need to be larger.
     * The measure call discovers the exact sizes needed, and we
     * reallocate accordingly. No compile-time constants required.  */

    printf("── Phase 4: Load v2 schema (measure -> allocate -> parse) ───\n\n");

    if (load_schema("hvac_v2.map", &schema, &ctx, &m) != 0) {
        return 1;
    }

    printf("Loaded: %s v%u (%zu entries)\n\n", schema.map_name, schema.version,
           schema.entry_count);

    print_memory_report("v2 (94 entries, 4 fstr, 7 str)", &m);

    /* ── Phase 5: Migrate v1 config into v2 context ──────────────────── */

    printf("── Phase 5: Migrate v1 data into v2 schema ──────────────────\n\n");

    if (strcmp(stored_name, schema.map_name) != 0) {
        /* Different schema — need remap */
        printf("  Schema changed: \"%s\" -> \"%s\"\n", stored_name,
               schema.map_name);
        printf("  Applying remap table (%zu entries):\n", REMAP_COUNT);
        for (size_t i = 0; i < REMAP_COUNT; i++) {
            printf("    old index %u -> new index %u\n",
                   v1_to_v2_remap[i].old_index, v1_to_v2_remap[i].new_index);
        }
        printf("\n");

        rc = cfgpack_pagein_remap(&ctx, flash, flash_len, v1_to_v2_remap,
                                  REMAP_COUNT);
        if (rc != CFGPACK_OK) {
            fprintf(stderr, "Remap pagein failed: %d\n", rc);
            return 1;
        }
        printf("  Migration successful.\n\n");
    } else {
        /* Same schema — direct load */
        rc = cfgpack_pagein_buf(&ctx, flash, flash_len);
        if (rc != CFGPACK_OK) {
            fprintf(stderr, "Pagein failed: %d\n", rc);
            return 1;
        }
    }

    /* ── Phase 6: Verify migration results ───────────────────────────── */

    printf("── Phase 6: Verify migration results ────────────────────────\n\n");

    int fail = 0;

    /* Check KEEP: z0en at index 1 should still be 1 */
    uint8_t z0en;
    cfgpack_get_u8(&ctx, 1, &z0en);
    printf("  KEEP   z0en  (idx  1)  = %u  %s\n", z0en,
           z0en == 1 ? "[OK]" : "[FAIL]");
    if (z0en != 1) {
        fail++;
    }

    /* Check WIDEN: z0sp at index 13 was u8=25 in v1, now u16=25 in v2 */
    uint16_t z0sp;
    cfgpack_get_u16(&ctx, 13, &z0sp);
    printf("  WIDEN  z0sp  (idx 13)  = %u  %s (u8 25 -> u16 25)\n", z0sp,
           z0sp == 25 ? "[OK]" : "[FAIL]");
    if (z0sp != 25) {
        fail++;
    }

    /* Check MOVE: ahi was at index 75 in v1, now at index 92 in v2 */
    uint8_t ahi;
    cfgpack_get_u8(&ctx, 92, &ahi);
    printf("  MOVE   ahi   (idx 92)  = %u  %s (was index 75)\n", ahi,
           ahi == 40 ? "[OK]" : "[FAIL]");
    if (ahi != 40) {
        fail++;
    }

    /* Check REMOVE: zaen at old index 11 does not exist in v2 */
    cfgpack_value_t tmp;
    rc = cfgpack_get(&ctx, 11, &tmp);
    printf("  REMOVE zaen  (idx 11)  = %s  %s\n",
           rc == CFGPACK_ERR_MISSING ? "<absent>" : "<present>",
           rc == CFGPACK_ERR_MISSING ? "[OK]" : "[FAIL]");
    if (rc != CFGPACK_ERR_MISSING) {
        fail++;
    }

    /* Check KEEP (fstr): dname at index 82 should still be "main" */
    const char *dname;
    uint8_t dname_len;
    cfgpack_get_fstr(&ctx, 82, &dname, &dname_len);
    printf("  KEEP   dname (idx 82)  = \"%.*s\"  %s\n", (int)dname_len, dname,
           (dname_len == 4 && memcmp(dname, "main", 4) == 0) ? "[OK]"
                                                             : "[FAIL]");
    if (dname_len != 4 || memcmp(dname, "main", 4) != 0) {
        fail++;
    }

    /* Check ADD: new v2 numeric entries should have their defaults */
    uint8_t h0sp;
    cfgpack_get_u8(&ctx, 85, &h0sp);
    printf("  ADD    h0sp  (idx 85)  = %u  %s (v2 default)\n", h0sp,
           h0sp == 50 ? "[OK]" : "[FAIL]");
    if (h0sp != 50) {
        fail++;
    }

    uint8_t eco;
    cfgpack_get_u8(&ctx, 96, &eco);
    printf("  ADD    eco   (idx 96)  = %u  %s (v2 default)\n", eco,
           eco == 0 ? "[OK]" : "[FAIL]");
    if (eco != 0) {
        fail++;
    }

    /* Check ADD (str): new zone names should have defaults */
    const char *zn0;
    uint16_t zn0_len;
    cfgpack_get_str(&ctx, 99, &zn0, &zn0_len);
    printf("  ADD    zn0   (idx 99)  = \"%.*s\"  %s (str default)\n",
           (int)zn0_len, zn0,
           (zn0_len == 5 && memcmp(zn0, "lobby", 5) == 0) ? "[OK]" : "[FAIL]");
    if (zn0_len != 5 || memcmp(zn0, "lobby", 5) != 0) {
        fail++;
    }

    const char *zn4;
    uint16_t zn4_len;
    cfgpack_get_str(&ctx, 103, &zn4, &zn4_len);
    printf("  ADD    zn4   (idx 103) = \"%.*s\"  %s (str default)\n",
           (int)zn4_len, zn4,
           (zn4_len == 11 && memcmp(zn4, "server-room", 11) == 0) ? "[OK]"
                                                                  : "[FAIL]");
    if (zn4_len != 11 || memcmp(zn4, "server-room", 11) != 0) {
        fail++;
    }

    /* Check ADD (fstr): firmware version */
    const char *fwver;
    uint8_t fwver_len;
    cfgpack_get_fstr(&ctx, 104, &fwver, &fwver_len);
    printf("  ADD    fwver (idx 104) = \"%.*s\"  %s (fstr default)\n",
           (int)fwver_len, fwver,
           (fwver_len == 5 && memcmp(fwver, "2.0.0", 5) == 0) ? "[OK]"
                                                              : "[FAIL]");
    if (fwver_len != 5 || memcmp(fwver, "2.0.0", 5) != 0) {
        fail++;
    }

    /* Check ADD (str): NTP host */
    const char *ntph;
    uint16_t ntph_len;
    cfgpack_get_str(&ctx, 105, &ntph, &ntph_len);
    printf("  ADD    ntph  (idx 105) = \"%.*s\"  %s (str default)\n",
           (int)ntph_len, ntph,
           (ntph_len == 12 && memcmp(ntph, "pool.ntp.org", 12) == 0)
               ? "[OK]"
               : "[FAIL]");
    if (ntph_len != 12 || memcmp(ntph, "pool.ntp.org", 12) != 0) {
        fail++;
    }

    /* Check ADD (str): contact email */
    const char *email;
    uint16_t email_len;
    cfgpack_get_str(&ctx, 106, &email, &email_len);
    printf("  ADD    email (idx 106) = \"%.*s\"  %s (str default)\n",
           (int)email_len, email,
           (email_len == 16 && memcmp(email, "hvac@example.com", 16) == 0)
               ? "[OK]"
               : "[FAIL]");
    if (email_len != 16 || memcmp(email, "hvac@example.com", 16) != 0) {
        fail++;
    }

    /* Check ADD: per-zone deadband */
    uint8_t db0;
    cfgpack_get_u8(&ctx, 107, &db0);
    printf("  ADD    db0   (idx 107) = %u  %s (v2 default)\n", db0,
           db0 == 10 ? "[OK]" : "[FAIL]");
    if (db0 != 10) {
        fail++;
    }

    printf("\n");

    /* ── Phase 7: Full dump of migrated config ───────────────────────── */

    printf("── Phase 7: Full v2 configuration after migration ───────────\n\n");
    dump_entries(&ctx);

    /* ── Summary ─────────────────────────────────────────────────────── */

    printf("── Summary ──────────────────────────────────────────────────\n\n");

    size_t alloc_total = m.entry_count * sizeof(cfgpack_entry_t) +
                         m.entry_count * sizeof(cfgpack_value_t) +
                         m.str_pool_size +
                         (m.str_count + m.fstr_count) * sizeof(uint16_t);

    printf("  CFGPACK_MAX_ENTRIES:               %d\n", CFGPACK_MAX_ENTRIES);
    printf("  v2 entry count (measured):         %zu\n", m.entry_count);
    printf("  v2 strings (measured):             %zu str + %zu fstr\n",
           m.str_count, m.fstr_count);
    printf("  Allocated buffers (from measure):  %zu bytes\n", alloc_total);
    printf("  Fixed overhead (ctx + schema):     %zu bytes\n",
           sizeof(cfgpack_ctx_t) + sizeof(cfgpack_schema_t));
    printf("  Schema parse buffer (reclaimable): %zu bytes\n", sizeof(map_buf));
    printf("  Flash storage used:                %zu bytes\n", flash_len);
    printf("  Entries migrated:                  %zu present\n",
           cfgpack_get_size(&ctx));

    if (fail > 0) {
        printf("\n  FAILED: %d check(s) did not pass.\n", fail);
    } else {
        printf("\n  Schema migration: hvac_v1 -> hvac_v2 complete. "
               "All checks passed.\n");
    }

    free_buffers();
    return fail > 0 ? 1 : 0;
}
