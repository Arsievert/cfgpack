#ifndef CFGPACK_API_H
#define CFGPACK_API_H

/**
 * @file api.h
 * @brief Runtime context and value access API for cfgpack.
 *
 * This header provides the runtime context structure and functions for:
 * - Initializing and managing configuration contexts
 * - Getting and setting values by index or name
 * - Typed convenience functions for all supported types
 * - Serializing/deserializing to MessagePack format
 *
 * All functions use caller-provided buffers (no heap allocation).
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "error.h"
#include "schema.h"
#include "value.h"

/**
 * @brief Reserved index for schema name.
 *
 * Index 0 is reserved in the MessagePack blob for storing the schema name
 * as a string. This allows version detection when loading config from flash.
 * User schema entries should start at index 1 or higher.
 */
#define CFGPACK_INDEX_RESERVED_NAME 0

/**
 * @brief Remap table entry for migrating config between schema versions.
 *
 * Used to translate indices from an old schema to a new schema when loading
 * config that was saved by a previous firmware version.
 */
typedef struct {
    uint16_t old_index; /**< Index in the old schema. */
    uint16_t new_index; /**< Corresponding index in the new schema. */
} cfgpack_remap_entry_t;

/**
 * @brief Runtime context using caller-owned buffers (no heap).
 *
 * String values are stored in an external pool rather than inline.
 * Use cfgpack_schema_get_sizing() to determine required pool sizes
 * before calling cfgpack_init().
 *
 * The presence bitmap is embedded in the context structure and supports
 * up to CFGPACK_MAX_ENTRIES entries (default 128, configurable in config.h).
 */
struct cfgpack_ctx {
    const cfgpack_schema_t
        *schema; /**< Pointer to schema describing entries. */
    cfgpack_value_t
        *values; /**< Caller-provided value slots (size = entry_count). */
    size_t values_count; /**< Number of value slots available. */
    uint8_t present
        [CFGPACK_PRESENCE_BYTES]; /**< Inline presence bitmap (entry_count bits). */
    char *str_pool;               /**< Caller-provided string pool buffer. */
    size_t str_pool_cap;          /**< Capacity of string pool in bytes. */
    uint16_t *str_offsets;    /**< Per-string-entry offsets into str_pool. */
    size_t str_offsets_count; /**< Number of string offset slots. */
};

/**
 * @brief Mark entry index as present in the context bitmap.
 * @param ctx Context with presence bitmap.
 * @param idx Entry index to mark present.
 */
static inline void cfgpack_presence_set(cfgpack_ctx_t *ctx, size_t idx) {
    ctx->present[idx / CHAR_BIT] |= (uint8_t)(1u << (idx % CHAR_BIT));
}

/**
 * @brief Test presence bit for entry index in the context bitmap.
 * @param ctx Context with presence bitmap.
 * @param idx Entry index to query.
 * @return 1 if present, 0 otherwise.
 */
static inline int cfgpack_presence_get(const cfgpack_ctx_t *ctx, size_t idx) {
    return (ctx->present[idx / CHAR_BIT] >> (idx % CHAR_BIT)) & 1u;
}

/**
 * @brief Clear presence bit for entry index in the context bitmap.
 * @param ctx Context with presence bitmap.
 * @param idx Entry index to clear.
 */
static inline void cfgpack_presence_clear(cfgpack_ctx_t *ctx, size_t idx) {
    ctx->present[idx / CHAR_BIT] &= (uint8_t)~(1u << (idx % CHAR_BIT));
}

/**
 * @brief Initialize context with caller buffers.
 *
 * Sets up the runtime context with references to the parsed schema,
 * value slots, string pool, and string offsets. The values and string pool
 * should already contain defaults from schema parsing.
 *
 * @param ctx              Context to initialize (output).
 * @param schema           Parsed schema describing entries; must outlive ctx.
 * @param values           Caller-owned array of value slots (>= entry_count).
 *                         Should already contain defaults from parsing.
 * @param values_count     Number of elements in @p values.
 * @param str_pool         Caller-owned string pool buffer (use cfgpack_schema_get_sizing()).
 * @param str_pool_cap     Capacity of @p str_pool in bytes.
 * @param str_offsets      Caller-owned array for string offsets (str_count + fstr_count).
 * @param str_offsets_count Number of elements in @p str_offsets.
 * @return CFGPACK_OK on success; CFGPACK_ERR_BOUNDS if buffers are too small
 *         or schema has more than CFGPACK_MAX_ENTRIES entries.
 */
cfgpack_err_t cfgpack_init(cfgpack_ctx_t *ctx,
                           const cfgpack_schema_t *schema,
                           cfgpack_value_t *values,
                           size_t values_count,
                           char *str_pool,
                           size_t str_pool_cap,
                           uint16_t *str_offsets,
                           size_t str_offsets_count);

/**
 * @brief No-op cleanup (buffers are caller-owned).
 * @param ctx Context to release (no-op).
 */
void cfgpack_free(cfgpack_ctx_t *ctx);


/**
 * @brief Set a value by schema index; validates type and string lengths.
 *
 * @param ctx    Initialized context.
 * @param index  Schema index to set.
 * @param value  Value to store (type must match schema entry).
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if index not in schema;
 *         CFGPACK_ERR_TYPE_MISMATCH for wrong type; CFGPACK_ERR_STR_TOO_LONG
 *         if string exceeds limits.
 */
cfgpack_err_t cfgpack_set(cfgpack_ctx_t *ctx,
                          uint16_t index,
                          const cfgpack_value_t *value);

/**
 * @brief Get a value by schema index; fails if not present.
 *
 * @param ctx       Initialized context.
 * @param index     Schema index to retrieve.
 * @param out_value Filled on success.
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if absent or unknown.
 */
cfgpack_err_t cfgpack_get(const cfgpack_ctx_t *ctx,
                          uint16_t index,
                          cfgpack_value_t *out_value);

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
cfgpack_err_t cfgpack_set_by_name(cfgpack_ctx_t *ctx,
                                  const char *name,
                                  const cfgpack_value_t *value);

/**
 * @brief Get a value by schema name; fails if not present.
 *
 * @param ctx       Initialized context.
 * @param name      Schema name to retrieve (NUL-terminated, up to 5 chars).
 * @param out_value Filled on success.
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if absent or unknown.
 */
cfgpack_err_t cfgpack_get_by_name(const cfgpack_ctx_t *ctx,
                                  const char *name,
                                  cfgpack_value_t *out_value);

/* ═══════════════════════════════════════════════════════════════════════════
 * Typed Setter Convenience Functions (by index)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Set a uint8_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_u8(cfgpack_ctx_t *ctx,
                                           uint16_t index,
                                           uint8_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set a uint16_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_u16(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            uint16_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U16, .v.u64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set a uint32_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_u32(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            uint32_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U32, .v.u64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set a uint64_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_u64(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            uint64_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U64, .v.u64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set an int8_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_i8(cfgpack_ctx_t *ctx,
                                           uint16_t index,
                                           int8_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I8, .v.i64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set an int16_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_i16(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            int16_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I16, .v.i64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set an int32_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_i32(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            int32_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I32, .v.i64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set an int64_t value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_i64(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            int64_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I64, .v.i64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set a float value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_f32(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            float val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_F32, .v.f32 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set a double value by index. @see cfgpack_set */
static inline cfgpack_err_t cfgpack_set_f64(cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            double val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_F64, .v.f64 = val};
    return cfgpack_set(ctx, index, &v);
}

/** @brief Set a variable-length string by index. @see cfgpack_set */
cfgpack_err_t cfgpack_set_str(cfgpack_ctx_t *ctx,
                              uint16_t index,
                              const char *str);

/** @brief Set a fixed-length string by index. @see cfgpack_set */
cfgpack_err_t cfgpack_set_fstr(cfgpack_ctx_t *ctx,
                               uint16_t index,
                               const char *str);

/* ═══════════════════════════════════════════════════════════════════════════
 * Typed Setter Convenience Functions (by name)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Set a uint8_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_u8_by_name(cfgpack_ctx_t *ctx,
                                                   const char *name,
                                                   uint8_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set a uint16_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_u16_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    uint16_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U16, .v.u64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set a uint32_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_u32_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    uint32_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U32, .v.u64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set a uint64_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_u64_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    uint64_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U64, .v.u64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set an int8_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_i8_by_name(cfgpack_ctx_t *ctx,
                                                   const char *name,
                                                   int8_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I8, .v.i64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set an int16_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_i16_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    int16_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I16, .v.i64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set an int32_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_i32_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    int32_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I32, .v.i64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set an int64_t value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_i64_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    int64_t val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_I64, .v.i64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set a float value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_f32_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    float val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_F32, .v.f32 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set a double value by name. @see cfgpack_set_by_name */
static inline cfgpack_err_t cfgpack_set_f64_by_name(cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    double val) {
    cfgpack_value_t v = {.type = CFGPACK_TYPE_F64, .v.f64 = val};
    return cfgpack_set_by_name(ctx, name, &v);
}

/** @brief Set a variable-length string by name. @see cfgpack_set_by_name */
cfgpack_err_t cfgpack_set_str_by_name(cfgpack_ctx_t *ctx,
                                      const char *name,
                                      const char *str);

/** @brief Set a fixed-length string by name. @see cfgpack_set_by_name */
cfgpack_err_t cfgpack_set_fstr_by_name(cfgpack_ctx_t *ctx,
                                       const char *name,
                                       const char *str);

/* ═══════════════════════════════════════════════════════════════════════════
 * Typed Getter Convenience Functions (by index)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Get a uint8_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_u8(const cfgpack_ctx_t *ctx,
                                           uint16_t index,
                                           uint8_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U8) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (uint8_t)v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get a uint16_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_u16(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            uint16_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U16) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (uint16_t)v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get a uint32_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_u32(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            uint32_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U32) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (uint32_t)v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get a uint64_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_u64(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            uint64_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U64) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get an int8_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_i8(const cfgpack_ctx_t *ctx,
                                           uint16_t index,
                                           int8_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I8) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (int8_t)v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get an int16_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_i16(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            int16_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I16) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (int16_t)v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get an int32_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_i32(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            int32_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I32) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (int32_t)v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get an int64_t value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_i64(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            int64_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I64) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get a float value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_f32(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            float *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_F32) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.f32;
    return CFGPACK_OK;
}

/** @brief Get a double value by index. @see cfgpack_get */
static inline cfgpack_err_t cfgpack_get_f64(const cfgpack_ctx_t *ctx,
                                            uint16_t index,
                                            double *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get(ctx, index, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_F64) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.f64;
    return CFGPACK_OK;
}

/** @brief Get a string pointer and length by index (from pool). @see cfgpack_get */
cfgpack_err_t cfgpack_get_str(const cfgpack_ctx_t *ctx,
                              uint16_t index,
                              const char **out,
                              uint16_t *len);

/** @brief Get a fixed-length string pointer and length by index (from pool). @see cfgpack_get */
cfgpack_err_t cfgpack_get_fstr(const cfgpack_ctx_t *ctx,
                               uint16_t index,
                               const char **out,
                               uint8_t *len);

/* ═══════════════════════════════════════════════════════════════════════════
 * Typed Getter Convenience Functions (by name)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Get a uint8_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_u8_by_name(const cfgpack_ctx_t *ctx,
                                                   const char *name,
                                                   uint8_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U8) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (uint8_t)v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get a uint16_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_u16_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    uint16_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U16) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (uint16_t)v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get a uint32_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_u32_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    uint32_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U32) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (uint32_t)v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get a uint64_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_u64_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    uint64_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_U64) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.u64;
    return CFGPACK_OK;
}

/** @brief Get an int8_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_i8_by_name(const cfgpack_ctx_t *ctx,
                                                   const char *name,
                                                   int8_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I8) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (int8_t)v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get an int16_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_i16_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    int16_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I16) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (int16_t)v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get an int32_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_i32_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    int32_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I32) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = (int32_t)v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get an int64_t value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_i64_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    int64_t *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_I64) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.i64;
    return CFGPACK_OK;
}

/** @brief Get a float value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_f32_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    float *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_F32) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.f32;
    return CFGPACK_OK;
}

/** @brief Get a double value by name. @see cfgpack_get_by_name */
static inline cfgpack_err_t cfgpack_get_f64_by_name(const cfgpack_ctx_t *ctx,
                                                    const char *name,
                                                    double *out) {
    cfgpack_value_t v;
    cfgpack_err_t rc = cfgpack_get_by_name(ctx, name, &v);
    if (rc != CFGPACK_OK) {
        return rc;
    }
    if (v.type != CFGPACK_TYPE_F64) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }
    *out = v.v.f64;
    return CFGPACK_OK;
}

/** @brief Get a string pointer and length by name (from pool). @see cfgpack_get_by_name */
cfgpack_err_t cfgpack_get_str_by_name(const cfgpack_ctx_t *ctx,
                                      const char *name,
                                      const char **out,
                                      uint16_t *len);

/** @brief Get a fixed-length string pointer and length by name (from pool). @see cfgpack_get_by_name */
cfgpack_err_t cfgpack_get_fstr_by_name(const cfgpack_ctx_t *ctx,
                                       const char *name,
                                       const char **out,
                                       uint8_t *len);

/**
 * @brief Encode present values into a MessagePack map in caller buffer.
 *
 * The schema name is automatically written at CFGPACK_INDEX_RESERVED_NAME (0)
 * to enable version detection when loading config from flash.
 *
 * @param ctx      Initialized context.
 * @param out      Output buffer for MessagePack payload.
 * @param out_cap  Capacity of @p out in bytes.
 * @param out_len  Optional length written (set on success).
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer too small.
 */
cfgpack_err_t cfgpack_pageout(const cfgpack_ctx_t *ctx,
                              uint8_t *out,
                              size_t out_cap,
                              size_t *out_len);

/**
 * @brief Peek at the schema name stored in a MessagePack config blob.
 *
 * Reads the schema name from CFGPACK_INDEX_RESERVED_NAME (0) without fully
 * decoding the blob. Use this to determine which remap table to apply when
 * loading config from a previous firmware version.
 *
 * @param data    MessagePack config blob.
 * @param len     Length of @p data in bytes.
 * @param out_name Output buffer for schema name (null-terminated).
 * @param out_cap  Capacity of @p out_name in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE if blob is malformed;
 *         CFGPACK_ERR_MISSING if key 0 not found; CFGPACK_ERR_BOUNDS if
 *         name doesn't fit in output buffer.
 */
cfgpack_err_t cfgpack_peek_name(const uint8_t *data,
                                size_t len,
                                char *out_name,
                                size_t out_cap);

/**
 * @brief Decode from a MessagePack buffer into the context with index remapping.
 *
 * Use this when loading config from flash that may have been saved by a
 * previous firmware version with a different schema. The remap table
 * translates old indices to new indices.
 *
 * Behavior:
 * - Key 0 (schema name) is skipped (not loaded as a config value)
 * - Keys in remap table are translated to new indices
 * - Keys not in schema (after remapping) are silently ignored
 * - Type widening is allowed (e.g., u8 value into u16 field)
 * - Type narrowing or incompatible types return CFGPACK_ERR_TYPE_MISMATCH
 *
 * @param ctx         Initialized context.
 * @param data        MessagePack map buffer.
 * @param len         Length of @p data in bytes.
 * @param remap       Remap table (NULL for no remapping).
 * @param remap_count Number of entries in @p remap.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on malformed input;
 *         CFGPACK_ERR_TYPE_MISMATCH on incompatible types;
 *         CFGPACK_ERR_STR_TOO_LONG if strings exceed limits.
 */
cfgpack_err_t cfgpack_pagein_remap(cfgpack_ctx_t *ctx,
                                   const uint8_t *data,
                                   size_t len,
                                   const cfgpack_remap_entry_t *remap,
                                   size_t remap_count);

/**
 * @brief Decode from a MessagePack buffer into the context.
 *
 * Equivalent to cfgpack_pagein_remap(ctx, data, len, NULL, 0).
 * Unknown keys are silently ignored for forward compatibility.
 *
 * @param ctx   Initialized context.
 * @param data  MessagePack map buffer.
 * @param len   Length of @p data in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on malformed input;
 *         CFGPACK_ERR_STR_TOO_LONG if strings exceed limits.
 */
cfgpack_err_t cfgpack_pagein_buf(cfgpack_ctx_t *ctx,
                                 const uint8_t *data,
                                 size_t len);

/**
 * @brief Print a single present value by index to stdout.
 *
 * @note In embedded mode (CFGPACK_EMBEDDED, the default), this function
 *       is a silent no-op that returns CFGPACK_OK. Compile with
 *       -DCFGPACK_HOSTED to enable actual printf output.
 *
 * @param ctx   Initialized context.
 * @param index Schema index to print.
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if absent or unknown
 *         (hosted mode only).
 */
cfgpack_err_t cfgpack_print(const cfgpack_ctx_t *ctx, uint16_t index);

/**
 * @brief Print all present values to stdout.
 *
 * @note In embedded mode (CFGPACK_EMBEDDED, the default), this function
 *       is a silent no-op that returns CFGPACK_OK. Compile with
 *       -DCFGPACK_HOSTED to enable actual printf output.
 *
 * @param ctx Initialized context.
 * @return CFGPACK_OK on success.
 */
cfgpack_err_t cfgpack_print_all(const cfgpack_ctx_t *ctx);

/**
 * @brief Return schema version.
 * @param ctx Initialized context.
 * @return Schema version from the map header.
 */
uint32_t cfgpack_get_version(const cfgpack_ctx_t *ctx);

/**
 * @brief Return count of present values.
 * @param ctx Initialized context.
 * @return Number of entries marked present.
 */
size_t cfgpack_get_size(const cfgpack_ctx_t *ctx);

#endif /* CFGPACK_API_H */
