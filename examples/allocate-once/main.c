/**
 * @file main.c
 * @brief CFGPack Dynamic Allocation Example
 *
 * Demonstrates:
 * - Heap-allocated buffers with malloc (no free — "allocate once" pattern)
 * - Two-phase init: discovery parse to learn sizes, then right-sized malloc
 * - cfgpack_schema_get_sizing() for computing exact buffer requirements
 * - The library doesn't care where memory comes from (stack, static, heap)
 *
 * Contrast with the datalogger and sensor_hub examples which use statically-
 * sized arrays. This example right-sizes everything at runtime, which is
 * useful when the schema is loaded from external storage and its size isn't
 * known at compile time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfgpack/cfgpack.h"

/*---------------------------------------------------------------------------*/
/* Helper to get type name string */
static const char *type_name(cfgpack_type_t t) {
    switch (t) {
    case CFGPACK_TYPE_U8: return ("u8");
    case CFGPACK_TYPE_U16: return ("u16");
    case CFGPACK_TYPE_U32: return ("u32");
    case CFGPACK_TYPE_U64: return ("u64");
    case CFGPACK_TYPE_I8: return ("i8");
    case CFGPACK_TYPE_I16: return ("i16");
    case CFGPACK_TYPE_I32: return ("i32");
    case CFGPACK_TYPE_I64: return ("i64");
    case CFGPACK_TYPE_F32: return ("f32");
    case CFGPACK_TYPE_F64: return ("f64");
    case CFGPACK_TYPE_STR: return ("str");
    case CFGPACK_TYPE_FSTR: return ("fstr");
    default: return ("???");
    }
}

/*---------------------------------------------------------------------------*/
/**
 * Generic config dump — iterates all entries with types discovered at runtime.
 */
static void dump_all_entries(const cfgpack_ctx_t *c) {
    printf("  %-6s %-5s %-5s %s\n", "INDEX", "NAME", "TYPE", "VALUE");
    printf("  ------ ----- ----- ----------------------------------------\n");

    for (size_t i = 0; i < c->schema->entry_count; i++) {
        const cfgpack_entry_t *e = &c->schema->entries[i];
        cfgpack_value_t val;

        if (cfgpack_get(c, e->index, &val) != CFGPACK_OK) {
            continue;
        }

        printf("  %-6u %-5s %-5s ", e->index, e->name, type_name(e->type));

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
int main(int argc, char **argv) {
    cfgpack_err_t rc;
    cfgpack_parse_error_t parse_err;
    const char *schema_path = "../datalogger/datalogger.map";

    if (argc > 1) {
        schema_path = argv[1];
    }

    /* ── 1. Load schema file into a stack buffer ───────────────────────── */

    printf("Loading schema from: %s\n", schema_path);

    char file_buf[4096];
    FILE *f = fopen(schema_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", schema_path);
        return (1);
    }
    size_t file_len = fread(file_buf, 1, sizeof(file_buf) - 1, f);
    fclose(f);
    file_buf[file_len] = '\0';

    /* ── 2. Discovery parse: learn entry count and sizing ──────────────
     *
     * We don't know how many entries the schema has at compile time, so
     * we parse once into oversized stack-local buffers just to discover
     * the entry count and string sizing. These temporaries are only used
     * briefly and can be large because they live on the stack for a short
     * time during startup. */

    cfgpack_schema_t disc_schema;
    cfgpack_entry_t disc_entries[CFGPACK_MAX_ENTRIES];
    cfgpack_value_t disc_values[CFGPACK_MAX_ENTRIES];
    char disc_str_pool[4096];
    uint16_t disc_str_offsets[CFGPACK_MAX_ENTRIES];

    rc = cfgpack_parse_schema(file_buf, file_len, &disc_schema, disc_entries,
                              CFGPACK_MAX_ENTRIES, disc_values, disc_str_pool,
                              sizeof(disc_str_pool), disc_str_offsets,
                              CFGPACK_MAX_ENTRIES, &parse_err);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Schema parse error at line %zu: %s\n", parse_err.line,
                parse_err.message);
        return (1);
    }

    /* Query sizing information */
    cfgpack_schema_sizing_t sizing;
    cfgpack_schema_get_sizing(&disc_schema, &sizing);

    size_t entry_count = disc_schema.entry_count;
    size_t str_offset_count = sizing.str_count + sizing.fstr_count;

    printf("Schema: %s v%u\n", disc_schema.map_name, disc_schema.version);
    printf("Discovered: %zu entries, %zu str + %zu fstr\n", entry_count,
           sizing.str_count, sizing.fstr_count);
    printf("Sizing:     str_pool=%zu bytes, str_offsets=%zu slots\n",
           sizing.str_pool_size, str_offset_count);
    printf(
        "Compare:    static example would reserve %d entries, %d str_offsets\n",
        CFGPACK_MAX_ENTRIES, CFGPACK_MAX_ENTRIES);
    printf("\n");

    /* ── 3. Malloc right-sized buffers ─────────────────────────────────
     *
     * In embedded systems that allocate once at boot and never deallocate,
     * this is a common pattern: malloc the exact sizes needed, then hold
     * the buffers for the lifetime of the program. No free() is needed
     * because the process (or MCU) never releases them. */

    cfgpack_entry_t *entries = malloc(entry_count * sizeof(cfgpack_entry_t));
    cfgpack_value_t *values = malloc(entry_count * sizeof(cfgpack_value_t));
    char *str_pool = NULL;
    uint16_t *str_offsets = NULL;

    if (sizing.str_pool_size > 0) {
        str_pool = malloc(sizing.str_pool_size);
    }
    if (str_offset_count > 0) {
        str_offsets = malloc(str_offset_count * sizeof(uint16_t));
    }

    if (!entries || !values || (sizing.str_pool_size > 0 && !str_pool) ||
        (str_offset_count > 0 && !str_offsets)) {
        fprintf(stderr, "malloc failed\n");
        return (1);
    }

    printf("Allocated:\n");
    printf("  entries:     %zu bytes (%zu x %zu)\n",
           entry_count * sizeof(cfgpack_entry_t), entry_count,
           sizeof(cfgpack_entry_t));
    printf("  values:      %zu bytes (%zu x %zu)\n",
           entry_count * sizeof(cfgpack_value_t), entry_count,
           sizeof(cfgpack_value_t));
    printf("  str_pool:    %zu bytes\n", sizing.str_pool_size);
    printf("  str_offsets: %zu bytes (%zu x %zu)\n",
           str_offset_count * sizeof(uint16_t), str_offset_count,
           sizeof(uint16_t));
    printf("\n");

    /* ── 4. Final parse into malloc'd buffers ──────────────────────────
     *
     * Re-parse the schema into our right-sized buffers. This also writes
     * default values directly into the values array and string pool. */

    cfgpack_schema_t schema;
    rc = cfgpack_parse_schema(file_buf, file_len, &schema, entries, entry_count,
                              values, str_pool, sizing.str_pool_size,
                              str_offsets, str_offset_count, &parse_err);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Final parse error at line %zu: %s\n", parse_err.line,
                parse_err.message);
        return (1);
    }

    /* ── 5. Initialize context ─────────────────────────────────────────
     *
     * cfgpack_init doesn't know or care that these buffers came from
     * malloc. The API is identical to the static-allocation examples. */

    cfgpack_ctx_t ctx;
    rc = cfgpack_init(&ctx, &schema, values, entry_count, str_pool,
                      sizing.str_pool_size, str_offsets, str_offset_count);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Init failed: %d\n", rc);
        return (1);
    }

    printf("--- Defaults ---\n");
    dump_all_entries(&ctx);

    /* ── 6. Modify some values ─────────────────────────────────────── */

    cfgpack_value_t val;

    val.type = CFGPACK_TYPE_U32;
    val.v.u64 = 5000;
    cfgpack_set_by_name(&ctx, "intv", &val);

    cfgpack_set_fstr_by_name(&ctx, "dname", "dyn-01");

    val.type = CFGPACK_TYPE_U16;
    val.v.u64 = 99;
    cfgpack_set_by_name(&ctx, "did", &val);

    printf("--- After modifications ---\n");
    dump_all_entries(&ctx);

    /* ── 7. Serialize to storage (pageout) ─────────────────────────── */

    uint8_t storage[512];
    size_t storage_len = 0;

    rc = cfgpack_pageout(&ctx, storage, sizeof(storage), &storage_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pageout failed: %d\n", rc);
        return (1);
    }
    printf("Serialized to %zu bytes of MessagePack\n\n", storage_len);

    /* ── 8. Simulate reboot: re-parse schema, re-init, pagein ──────── */

    rc = cfgpack_parse_schema(file_buf, file_len, &schema, entries, entry_count,
                              values, str_pool, sizing.str_pool_size,
                              str_offsets, str_offset_count, &parse_err);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Re-parse error: %s\n", parse_err.message);
        return (1);
    }

    rc = cfgpack_init(&ctx, &schema, values, entry_count, str_pool,
                      sizing.str_pool_size, str_offsets, str_offset_count);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Re-init failed: %d\n", rc);
        return (1);
    }

    rc = cfgpack_pagein_buf(&ctx, storage, storage_len);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Pagein failed: %d\n", rc);
        return (1);
    }

    printf("--- After simulated reboot + pagein ---\n");
    dump_all_entries(&ctx);

    printf("Round-trip successful!\n");

    /* No free() — in an embedded system these buffers are held for the
     * lifetime of the program. The OS reclaims memory on process exit. */

    return (0);
}
