#ifndef CFGPACK_API_H
#define CFGPACK_API_H

#include <stddef.h>

#include "error.h"
#include "schema.h"
#include "value.h"

/** \brief Runtime context using caller-owned buffers (no heap). */
typedef struct {
    const cfgpack_schema_t *schema; /**< Pointer to schema describing entries. */
    cfgpack_value_t *values;        /**< Caller-provided value slots (size = entry_count). */
    size_t values_count;            /**< Number of value slots available. */
    uint8_t *present;               /**< Presence bitmap (entry_count bits). */
    size_t present_bytes;           /**< Size of presence bitmap in bytes. */
} cfgpack_ctx_t;

/** \brief Mark entry index as present in the context bitmap. */
static inline void cfgpack_presence_set(cfgpack_ctx_t *ctx, size_t idx) {
    ctx->present[idx / 8] |= (uint8_t)(1u << (idx % 8));
}

/** \brief Test presence bit for entry index in the context bitmap. */
static inline int cfgpack_presence_get(const cfgpack_ctx_t *ctx, size_t idx) {
    return (ctx->present[idx / 8] >> (idx % 8)) & 1u;
}

/**
 * \brief Initialize context with caller buffers; zeroes values and presence.
 *
 * @param ctx            Context to initialize (output).
 * @param schema         Parsed schema describing entries; must outlive ctx.
 * @param values         Caller-owned array of value slots (>= entry_count).
 * @param values_count   Number of elements in \p values.
 * @param present        Caller-owned bitmap buffer (>= (entry_count+7)/8 bytes).
 * @param present_bytes  Size of \p present in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_BOUNDS if buffers are too small.
 */
cfgpack_err_t cfgpack_init(cfgpack_ctx_t *ctx, const cfgpack_schema_t *schema, cfgpack_value_t *values, size_t values_count, uint8_t *present, size_t present_bytes);

/** \brief No-op cleanup (buffers are caller-owned). */
void cfgpack_free(cfgpack_ctx_t *ctx);

/**
 * \brief Set a value by schema index; validates type and string lengths.
 *
 * @param ctx    Initialized context.
 * @param index  Schema index to set.
 * @param value  Value to store (type must match schema entry).
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if index not in schema;
 *         CFGPACK_ERR_TYPE_MISMATCH for wrong type; CFGPACK_ERR_STR_TOO_LONG
 *         if string exceeds limits.
 */
cfgpack_err_t cfgpack_set(cfgpack_ctx_t *ctx, uint16_t index, const cfgpack_value_t *value);

/**
 * \brief Get a value by schema index; fails if not present.
 *
 * @param ctx       Initialized context.
 * @param index     Schema index to retrieve.
 * @param out_value Filled on success.
 * @return CFGPACK_OK on success; CFGPACK_ERR_MISSING if absent or unknown.
 */
cfgpack_err_t cfgpack_get(const cfgpack_ctx_t *ctx, uint16_t index, cfgpack_value_t *out_value);

/**
 * \brief Encode present values into a MessagePack map in caller buffer.
 *
 * @param ctx      Initialized context.
 * @param out      Output buffer for MessagePack payload.
 * @param out_cap  Capacity of \p out in bytes.
 * @param out_len  Optional length written (set on success).
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer too small.
 */
cfgpack_err_t cfgpack_pageout(const cfgpack_ctx_t *ctx, uint8_t *out, size_t out_cap, size_t *out_len);

/**
 * \brief Encode to a file using caller scratch buffer (no heap).
 *
 * @param ctx          Initialized context.
 * @param path         Destination file path.
 * @param scratch      Scratch buffer used for encode.
 * @param scratch_cap  Capacity of \p scratch in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if scratch too small;
 *         CFGPACK_ERR_IO on write failures.
 */
cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);

/**
 * \brief Decode from a MessagePack buffer into the context.
 *
 * Resets presence bitmap before applying decoded values.
 * @param ctx   Initialized context.
 * @param data  MessagePack map buffer.
 * @param len   Length of \p data in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on malformed input or
 *         unknown keys; CFGPACK_ERR_STR_TOO_LONG if strings exceed limits.
 */
cfgpack_err_t cfgpack_pagein_buf(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * \brief Decode from a file using caller scratch buffer (no heap).
 *
 * @param ctx          Initialized context.
 * @param path         Source file path.
 * @param scratch      Scratch buffer to hold file contents.
 * @param scratch_cap  Capacity of \p scratch.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read/size errors;
 *         CFGPACK_ERR_DECODE if payload is invalid.
 */
cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);

/** \brief Print a single present value by index to stdout. */
cfgpack_err_t cfgpack_print(const cfgpack_ctx_t *ctx, uint16_t index);

/** \brief Print all present values to stdout. */
cfgpack_err_t cfgpack_print_all(const cfgpack_ctx_t *ctx);

/** \brief Return schema version. */
uint32_t cfgpack_get_version(const cfgpack_ctx_t *ctx);

/** \brief Return count of present values. */
size_t   cfgpack_get_size(const cfgpack_ctx_t *ctx);

#endif /* CFGPACK_API_H */
