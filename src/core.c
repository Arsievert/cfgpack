#include "cfgpack/api.h"
#include "cfgpack/config.h"

#include <string.h>

/**
 * @brief Find a schema entry by index using binary search.
 *
 * @param schema Schema containing sorted entries.
 * @param index  Entry index to locate.
 * @return Pointer to entry or NULL if not found.
 */
static const cfgpack_entry_t *find_entry(const cfgpack_schema_t *schema,
                                         uint16_t index) {
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
static const cfgpack_entry_t *find_entry_by_name(const cfgpack_schema_t *schema,
                                                 const char *name) {
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
static size_t entry_offset(const cfgpack_schema_t *schema,
                           const cfgpack_entry_t *entry) {
    return (size_t)(entry - schema->entries);
}

/**
 * @brief Get the string slot index for a given entry offset.
 *
 * String entries are numbered sequentially (str and fstr interleaved).
 * Returns the slot index for this entry, or -1 if not a string type.
 */
static int get_str_slot(const cfgpack_schema_t *schema, size_t entry_off) {
    int slot = 0;
    for (size_t i = 0; i < entry_off; ++i) {
        cfgpack_type_t t = schema->entries[i].type;
        if (t == CFGPACK_TYPE_STR || t == CFGPACK_TYPE_FSTR) {
            slot++;
        }
    }
    cfgpack_type_t t = schema->entries[entry_off].type;
    if (t != CFGPACK_TYPE_STR && t != CFGPACK_TYPE_FSTR) {
        return -1;
    }
    return slot;
}

cfgpack_err_t cfgpack_init(cfgpack_ctx_t *ctx,
                           const cfgpack_schema_t *schema,
                           cfgpack_value_t *values,
                           size_t values_count,
                           char *str_pool,
                           size_t str_pool_cap,
                           uint16_t *str_offsets,
                           size_t str_offsets_count) {
    if (schema->entry_count > CFGPACK_MAX_ENTRIES) {
        return CFGPACK_ERR_BOUNDS;
    }

    if (values_count < schema->entry_count) {
        return CFGPACK_ERR_BOUNDS;
    }

    /* Compute string slot offsets and verify pool capacity */
    size_t str_slot = 0;
    size_t pool_offset = 0;
    for (size_t i = 0; i < schema->entry_count; ++i) {
        cfgpack_type_t t = schema->entries[i].type;
        if (t == CFGPACK_TYPE_STR) {
            if (str_slot >= str_offsets_count) {
                return CFGPACK_ERR_BOUNDS;
            }
            str_offsets[str_slot] = (uint16_t)pool_offset;
            str_slot++;
            pool_offset += CFGPACK_STR_MAX + 1;
        } else if (t == CFGPACK_TYPE_FSTR) {
            if (str_slot >= str_offsets_count) {
                return CFGPACK_ERR_BOUNDS;
            }
            str_offsets[str_slot] = (uint16_t)pool_offset;
            str_slot++;
            pool_offset += CFGPACK_FSTR_MAX + 1;
        }
    }
    if (pool_offset > str_pool_cap) {
        return CFGPACK_ERR_BOUNDS;
    }

    /* Set up context fields — values and str_pool already contain defaults
     * from schema parsing, so we must NOT zero them. */
    memset(ctx->present, 0, sizeof(ctx->present));
    ctx->schema = schema;
    ctx->values = values;
    ctx->values_count = values_count;
    ctx->str_pool = str_pool;
    ctx->str_pool_cap = str_pool_cap;
    ctx->str_offsets = str_offsets;
    ctx->str_offsets_count = str_offsets_count;

    /* Mark entries with defaults as present */
    for (size_t i = 0; i < schema->entry_count; ++i) {
        if (schema->entries[i].has_default) {
            cfgpack_presence_set(ctx, i);
        }
    }

    return CFGPACK_OK;
}

void cfgpack_free(cfgpack_ctx_t *ctx) {
    (void)ctx; /* no-op: caller owns buffers */
}

static int type_matches(cfgpack_type_t expect, const cfgpack_value_t *v) {
    return expect == v->type;
}

cfgpack_err_t cfgpack_set(cfgpack_ctx_t *ctx,
                          uint16_t index,
                          const cfgpack_value_t *value) {
    const cfgpack_entry_t *entry;
    size_t off;

    if (index == 0) {
        return CFGPACK_ERR_RESERVED_INDEX;
    }
    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    if (!type_matches(entry->type, value)) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    if (value->type == CFGPACK_TYPE_STR && value->v.str.len > CFGPACK_STR_MAX) {
        return CFGPACK_ERR_STR_TOO_LONG;
    }
    if (value->type == CFGPACK_TYPE_FSTR &&
        value->v.fstr.len > CFGPACK_FSTR_MAX) {
        return CFGPACK_ERR_STR_TOO_LONG;
    }
    off = entry_offset(ctx->schema, entry);
    ctx->values[off] = *value;
    cfgpack_presence_set(ctx, off);
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_get(const cfgpack_ctx_t *ctx,
                          uint16_t index,
                          cfgpack_value_t *out_value) {
    const cfgpack_entry_t *entry;
    size_t off;

    if (index == 0) {
        return CFGPACK_ERR_RESERVED_INDEX;
    }
    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return CFGPACK_ERR_MISSING;
    }
    *out_value = ctx->values[off];
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_set_by_name(cfgpack_ctx_t *ctx,
                                  const char *name,
                                  const cfgpack_value_t *value) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    size_t off;

    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    if (!type_matches(entry->type, value)) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    if (value->type == CFGPACK_TYPE_STR && value->v.str.len > CFGPACK_STR_MAX) {
        return CFGPACK_ERR_STR_TOO_LONG;
    }
    if (value->type == CFGPACK_TYPE_FSTR &&
        value->v.fstr.len > CFGPACK_FSTR_MAX) {
        return CFGPACK_ERR_STR_TOO_LONG;
    }
    off = entry_offset(ctx->schema, entry);
    ctx->values[off] = *value;
    cfgpack_presence_set(ctx, off);
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_get_by_name(const cfgpack_ctx_t *ctx,
                                  const char *name,
                                  cfgpack_value_t *out_value) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    size_t off;

    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return CFGPACK_ERR_MISSING;
    }
    *out_value = ctx->values[off];
    return CFGPACK_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * String Setter/Getter Implementations (use string pool)
 * ═══════════════════════════════════════════════════════════════════════════ */

cfgpack_err_t cfgpack_set_str(cfgpack_ctx_t *ctx,
                              uint16_t index,
                              const char *str) {
    const cfgpack_entry_t *entry;
    size_t off, len;

    if (index == 0) {
        return CFGPACK_ERR_RESERVED_INDEX;
    }

    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    if (entry->type != CFGPACK_TYPE_STR) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }

    len = strlen(str);
    if (len > CFGPACK_STR_MAX) {
        return CFGPACK_ERR_STR_TOO_LONG;
    }

    off = entry_offset(ctx->schema, entry);
    int slot = get_str_slot(ctx->schema, off);
    if (slot < 0 || (size_t)slot >= ctx->str_offsets_count) {
        return CFGPACK_ERR_BOUNDS;
    }

    uint16_t pool_off = ctx->str_offsets[slot];
    char *dst = ctx->str_pool + pool_off;
    memcpy(dst, str, len);
    dst[len] = '\0';

    ctx->values[off].type = CFGPACK_TYPE_STR;
    ctx->values[off].v.str.offset = pool_off;
    ctx->values[off].v.str.len = (uint16_t)len;
    cfgpack_presence_set(ctx, off);

    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_set_fstr(cfgpack_ctx_t *ctx,
                               uint16_t index,
                               const char *str) {
    const cfgpack_entry_t *entry;
    size_t off, len;

    if (index == 0) {
        return CFGPACK_ERR_RESERVED_INDEX;
    }

    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    if (entry->type != CFGPACK_TYPE_FSTR) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }

    len = strlen(str);
    if (len > CFGPACK_FSTR_MAX) {
        return CFGPACK_ERR_STR_TOO_LONG;
    }

    off = entry_offset(ctx->schema, entry);
    int slot = get_str_slot(ctx->schema, off);
    if (slot < 0 || (size_t)slot >= ctx->str_offsets_count) {
        return CFGPACK_ERR_BOUNDS;
    }

    uint16_t pool_off = ctx->str_offsets[slot];
    char *dst = ctx->str_pool + pool_off;
    memcpy(dst, str, len);
    dst[len] = '\0';

    ctx->values[off].type = CFGPACK_TYPE_FSTR;
    ctx->values[off].v.fstr.offset = pool_off;
    ctx->values[off].v.fstr.len = (uint8_t)len;
    cfgpack_presence_set(ctx, off);

    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_set_str_by_name(cfgpack_ctx_t *ctx,
                                      const char *name,
                                      const char *str) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    return cfgpack_set_str(ctx, entry->index, str);
}

cfgpack_err_t cfgpack_set_fstr_by_name(cfgpack_ctx_t *ctx,
                                       const char *name,
                                       const char *str) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    return cfgpack_set_fstr(ctx, entry->index, str);
}

cfgpack_err_t cfgpack_get_str(const cfgpack_ctx_t *ctx,
                              uint16_t index,
                              const char **out,
                              uint16_t *len) {
    const cfgpack_entry_t *entry;
    size_t off;

    if (index == 0) {
        return CFGPACK_ERR_RESERVED_INDEX;
    }

    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    if (entry->type != CFGPACK_TYPE_STR) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }

    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return CFGPACK_ERR_MISSING;
    }

    *out = ctx->str_pool + ctx->values[off].v.str.offset;
    *len = ctx->values[off].v.str.len;
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_get_fstr(const cfgpack_ctx_t *ctx,
                               uint16_t index,
                               const char **out,
                               uint8_t *len) {
    const cfgpack_entry_t *entry;
    size_t off;

    if (index == 0) {
        return CFGPACK_ERR_RESERVED_INDEX;
    }

    entry = find_entry(ctx->schema, index);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    if (entry->type != CFGPACK_TYPE_FSTR) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }

    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return CFGPACK_ERR_MISSING;
    }

    *out = ctx->str_pool + ctx->values[off].v.fstr.offset;
    *len = ctx->values[off].v.fstr.len;
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_get_str_by_name(const cfgpack_ctx_t *ctx,
                                      const char *name,
                                      const char **out,
                                      uint16_t *len) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    return cfgpack_get_str(ctx, entry->index, out, len);
}

cfgpack_err_t cfgpack_get_fstr_by_name(const cfgpack_ctx_t *ctx,
                                       const char *name,
                                       const char **out,
                                       uint8_t *len) {
    const cfgpack_entry_t *entry = find_entry_by_name(ctx->schema, name);
    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    return cfgpack_get_fstr(ctx, entry->index, out, len);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t cfgpack_get_version(const cfgpack_ctx_t *ctx) {
    return ctx->schema->version;
}

size_t cfgpack_get_size(const cfgpack_ctx_t *ctx) {
    size_t count = 0;
    for (size_t i = 0; i < ctx->schema->entry_count; ++i) {
        if (cfgpack_presence_get(ctx, i)) {
            count++;
        }
    }
    return count;
}

#ifdef CFGPACK_HOSTED
  #include <stdio.h>

static void print_value(const cfgpack_ctx_t *ctx, size_t entry_off) {
    const cfgpack_value_t *v = &ctx->values[entry_off];
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
    case CFGPACK_TYPE_STR:
        printf("%.*s", v->v.str.len, ctx->str_pool + v->v.str.offset);
        break;
    case CFGPACK_TYPE_FSTR:
        printf("%.*s", v->v.fstr.len, ctx->str_pool + v->v.fstr.offset);
        break;
    }
}

cfgpack_err_t cfgpack_print(const cfgpack_ctx_t *ctx, uint16_t index) {
    const cfgpack_entry_t *entry = find_entry(ctx->schema, index);
    size_t off;

    if (!entry) {
        return CFGPACK_ERR_MISSING;
    }
    off = entry_offset(ctx->schema, entry);
    if (!cfgpack_presence_get(ctx, off)) {
        return CFGPACK_ERR_MISSING;
    }
    printf("[%u] %s = ", entry->index, entry->name);
    print_value(ctx, off);
    printf("\n");
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_print_all(const cfgpack_ctx_t *ctx) {
    for (size_t i = 0; i < ctx->schema->entry_count; ++i) {
        if (!cfgpack_presence_get(ctx, i)) {
            continue;
        }
        const cfgpack_entry_t *e = &ctx->schema->entries[i];
        printf("[%u] %s = ", e->index, e->name);
        print_value(ctx, i);
        printf("\n");
    }
    return CFGPACK_OK;
}

#else /* CFGPACK_EMBEDDED */

cfgpack_err_t cfgpack_print(const cfgpack_ctx_t *ctx, uint16_t index) {
    (void)ctx;
    (void)index;
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_print_all(const cfgpack_ctx_t *ctx) {
    (void)ctx;
    return CFGPACK_OK;
}

#endif /* CFGPACK_HOSTED */
