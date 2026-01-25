#include "cfgpack/api.h"

#include <string.h>
#include <stdio.h>

/**
 * @brief Find a schema entry by index using binary search.
 *
 * @param schema Schema containing sorted entries.
 * @param index  Entry index to locate.
 * @return Pointer to entry or NULL if not found.
 */
static const cfgpack_entry_t *find_entry(const cfgpack_schema_t *schema, uint16_t index) {
    size_t lo = 0;
    size_t hi = schema->entry_count;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        uint16_t mid_idx = schema->entries[mid].index;
        if (mid_idx == index) {
            return &schema->entries[mid];
        }
        if (mid_idx < index) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

/**
 * @brief Find a schema entry by name using linear search.
 *
 * @param schema Schema containing entries.
 * @param name   Entry name to locate (NUL-terminated).
 * @return Pointer to entry or NULL if not found.
 */
static const cfgpack_entry_t *find_entry_by_name(const cfgpack_schema_t *schema, const char *name) {
    for (size_t i = 0; i < schema->entry_count; ++i) {
        if (strcmp(schema->entries[i].name, name) == 0) {
            return &schema->entries[i];
        }
    }
    return NULL;
}

/**
 * @brief Compute offset of an entry within the schema entries array.
 *
 * @param schema Schema containing the entry.
 * @param entry  Entry pointer inside @p schema.
 * @return Zero-based offset for the entry.
 */
static size_t entry_offset(const cfgpack_schema_t *schema, const cfgpack_entry_t *entry) {
    return (size_t)(entry - schema->entries);
}

cfgpack_err_t cfgpack_init(cfgpack_ctx_t *ctx, const cfgpack_schema_t *schema, cfgpack_value_t *values, size_t values_count, const cfgpack_value_t *defaults, uint8_t *present, size_t present_bytes) {
    size_t needed_bits = (schema->entry_count + 7) / 8;

    memset(ctx, 0, sizeof(*ctx));
    ctx->schema = schema;
    ctx->values = values;
    ctx->values_count = values_count;
    ctx->defaults = defaults;
    ctx->present = present;
    ctx->present_bytes = present_bytes;

    if (values_count < schema->entry_count || present_bytes < needed_bits) {
        return (CFGPACK_ERR_BOUNDS);
    }
    memset(ctx->values, 0, values_count * sizeof(cfgpack_value_t));
    memset(ctx->present, 0, present_bytes);

    /* Apply default values for entries that have them */
    for (size_t i = 0; i < schema->entry_count; ++i) {
        if (schema->entries[i].has_default) {
            ctx->values[i] = defaults[i];
            cfgpack_presence_set(ctx, i);
        }
    }

    return (CFGPACK_OK);
}

void cfgpack_free(cfgpack_ctx_t *ctx) {
    (void)ctx; /* no-op: caller owns buffers */
}

void cfgpack_reset_to_defaults(cfgpack_ctx_t *ctx) {
    const cfgpack_schema_t *schema = ctx->schema;

    /* Clear all values and presence bits */
    memset(ctx->values, 0, ctx->values_count * sizeof(cfgpack_value_t));
    memset(ctx->present, 0, ctx->present_bytes);

    /* Re-apply default values for entries that have them */
    for (size_t i = 0; i < schema->entry_count; ++i) {
        if (schema->entries[i].has_default) {
            ctx->values[i] = ctx->defaults[i];
            cfgpack_presence_set(ctx, i);
        }
    }
}

static int type_matches(cfgpack_type_t expect, const cfgpack_value_t *v) {
    return expect == v->type;
}

cfgpack_err_t cfgpack_set(cfgpack_ctx_t *ctx, uint16_t index, const cfgpack_value_t *value) {
    const cfgpack_entry_t *entry;
    size_t off;

    if (index == 0) {
        return (CFGPACK_ERR_RESERVED_INDEX);
    }
    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return (CFGPACK_ERR_MISSING);
    }
    if (!type_matches(entry->type, value)) {
        return (CFGPACK_ERR_TYPE_MISMATCH);
    }
    if (value->type == CFGPACK_TYPE_STR && value->v.str.len > CFGPACK_STR_MAX) {
        return (CFGPACK_ERR_STR_TOO_LONG);
    }
    if (value->type == CFGPACK_TYPE_FSTR && value->v.fstr.len > CFGPACK_FSTR_MAX) {
        return (CFGPACK_ERR_STR_TOO_LONG);
    }
    off = entry_offset(ctx->schema, entry);
    ctx->values[off] = *value;
    cfgpack_presence_set(ctx, off);
    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_get(const cfgpack_ctx_t *ctx, uint16_t index, cfgpack_value_t *out_value) {
    const cfgpack_entry_t *entry;
    size_t off;

    if (index == 0) {
        return (CFGPACK_ERR_RESERVED_INDEX);
    }
    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return (CFGPACK_ERR_MISSING);
    }
    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return (CFGPACK_ERR_MISSING);
    }
    *out_value = ctx->values[off];
    return (CFGPACK_OK);
}

/**
 * @brief Set a value by schema name; validates type and string lengths.
 *
 * @param ctx   Initialized context.
 * @param name  Schema name to set (NUL-terminated, up to 5 chars).
 * @param value Value to store (type must match schema entry).
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if name not in schema;
 *         CFGPACK_ERR_TYPE_MISMATCH for wrong type; CFGPACK_ERR_STR_TOO_LONG
 *         if string exceeds limits.
 */
cfgpack_err_t cfgpack_set_by_name(cfgpack_ctx_t *ctx, const char *name, const cfgpack_value_t *value) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    size_t off;

    if (!entry) {
        return (CFGPACK_ERR_MISSING);
    }
    if (!type_matches(entry->type, value)) {
        return (CFGPACK_ERR_TYPE_MISMATCH);
    }
    if (value->type == CFGPACK_TYPE_STR && value->v.str.len > CFGPACK_STR_MAX) {
        return (CFGPACK_ERR_STR_TOO_LONG);
    }
    if (value->type == CFGPACK_TYPE_FSTR && value->v.fstr.len > CFGPACK_FSTR_MAX) {
        return (CFGPACK_ERR_STR_TOO_LONG);
    }
    off = entry_offset(ctx->schema, entry);
    ctx->values[off] = *value;
    cfgpack_presence_set(ctx, off);
    return (CFGPACK_OK);
}

/**
 * @brief Get a value by schema name; fails if not present.
 *
 * @param ctx       Initialized context.
 * @param name      Schema name to retrieve (NUL-terminated, up to 5 chars).
 * @param out_value Filled on success.
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if absent or unknown.
 */
cfgpack_err_t cfgpack_get_by_name(const cfgpack_ctx_t *ctx, const char *name, cfgpack_value_t *out_value) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    size_t off;

    if (!entry) {
        return (CFGPACK_ERR_MISSING);
    }
    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return (CFGPACK_ERR_MISSING);
    }
    *out_value = ctx->values[off];
    return (CFGPACK_OK);
}

uint32_t cfgpack_get_version(const cfgpack_ctx_t *ctx) {
    return (ctx->schema->version);
}

size_t cfgpack_get_size(const cfgpack_ctx_t *ctx) {
    size_t count = 0;
    for (size_t i = 0; i < ctx->schema->entry_count; ++i) {
        if (cfgpack_presence_get(ctx, i)) count++;
    }
    return count;
}

static void print_value(const cfgpack_value_t *v) {
    switch (v->type) {
        case CFGPACK_TYPE_U8: printf("%u", (unsigned)v->v.u64); break;
        case CFGPACK_TYPE_U16: printf("%u", (unsigned)v->v.u64); break;
        case CFGPACK_TYPE_U32: printf("%u", (unsigned)v->v.u64); break;
        case CFGPACK_TYPE_U64: printf("%llu", (unsigned long long)v->v.u64); break;
        case CFGPACK_TYPE_I8: printf("%d", (int)v->v.i64); break;
        case CFGPACK_TYPE_I16: printf("%d", (int)v->v.i64); break;
        case CFGPACK_TYPE_I32: printf("%d", (int)v->v.i64); break;
        case CFGPACK_TYPE_I64: printf("%lld", (long long)v->v.i64); break;
        case CFGPACK_TYPE_F32: printf("%f", v->v.f32); break;
        case CFGPACK_TYPE_F64: printf("%lf", v->v.f64); break;
        case CFGPACK_TYPE_STR: printf("%.*s", v->v.str.len, v->v.str.data); break;
        case CFGPACK_TYPE_FSTR: printf("%.*s", v->v.fstr.len, v->v.fstr.data); break;
    }
}

cfgpack_err_t cfgpack_print(const cfgpack_ctx_t *ctx, uint16_t index) {
    const cfgpack_entry_t *entry = find_entry(ctx->schema, index);
    size_t off;

    if (!entry) {
        return (CFGPACK_ERR_MISSING);
    }
    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return (CFGPACK_ERR_MISSING);
    }
    printf("[%u] %s = ", entry->index, entry->name);
    print_value(&ctx->values[off]);
    printf("\n");
    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_print_all(const cfgpack_ctx_t *ctx) {
    for (size_t i = 0; i < ctx->schema->entry_count; ++i) {
        if (!cfgpack_presence_get(ctx, i)) continue;
        const cfgpack_entry_t *e = &ctx->schema->entries[i];
        printf("[%u] %s = ", e->index, e->name);
        print_value(&ctx->values[i]);
        printf("\n");
    }
    return (CFGPACK_OK);
}
