#ifndef CFGPACK_SCHEMA_H
#define CFGPACK_SCHEMA_H

/**
 * @file schema.h
 * @brief Schema parsing and serialization for cfgpack.
 *
 * Provides functions to parse schemas from .map and JSON formats,
 * and to serialize schemas to JSON format.
 * All functions use caller-provided buffers (no heap allocation).
 */

#include <stddef.h>
#include <stdint.h>

#include "value.h"
#include "error.h"

/**
 * @brief Single entry within a schema.
 */
typedef struct {
    uint16_t index;
    char name[6]; /* 5 chars + null */
    cfgpack_type_t type;
    uint8_t has_default; /* 1 if default value exists, 0 otherwise */
} cfgpack_entry_t;

/**
 * @brief Parsed schema containing metadata and entries.
 */
typedef struct {
    char map_name[64];
    uint32_t version;
    cfgpack_entry_t *entries;
    size_t entry_count;
} cfgpack_schema_t;

/**
 * @brief Parse error details for schema parsing.
 */
typedef struct {
    char message[128];
    size_t line;
} cfgpack_parse_error_t;

/**
 * @brief Schema sizing information for memory allocation.
 *
 * Use cfgpack_schema_get_sizing() to compute the required buffer sizes
 * for string pool and offsets array before calling cfgpack_init().
 */
typedef struct {
    size_t str_pool_size; /**< Total bytes needed for string pool */
    size_t str_count;     /**< Number of str-type entries */
    size_t fstr_count;    /**< Number of fstr-type entries */
} cfgpack_schema_sizing_t;

/**
 * @brief Compute sizing information for a parsed schema.
 *
 * Call this after cfgpack_parse_schema() to determine how much memory
 * to allocate for the string pool and offsets array.
 *
 * @param schema Parsed schema.
 * @param out    Filled with sizing information.
 * @return CFGPACK_OK on success.
 */
cfgpack_err_t cfgpack_schema_get_sizing(const cfgpack_schema_t *schema, cfgpack_schema_sizing_t *out);

/**
 * @brief Parse a schema from a buffer into caller-provided buffers (no heap).
 *
 * Expected format:
 *   - Header: "<name> <version>" on the first non-comment line.
 *   - Entries: "<index> <name> <type> <default>" on subsequent non-comment lines.
 *   - Default values: NIL (no default), integer literals, float literals, or "quoted strings".
 * Comments (lines starting with '#') and blank lines are ignored.
 * Fails on duplicates (index or name), name length >5, unsupported type,
 * too many entries, or indices outside 0..65535.
 *
 * Default values are written directly into @p values and @p str_pool.
 * Entries with defaults are marked present in the output.
 *
 * @param data             Input buffer containing schema data (null-terminated).
 * @param data_len         Length of data in bytes.
 * @param out_schema       Filled on success; points at caller-owned entries array.
 * @param entries          Caller-owned array to store parsed entries.
 * @param max_entries      Capacity of @p entries.
 * @param values           Caller-owned value slots to receive defaults (>= max_entries).
 * @param str_pool         Caller-owned string pool buffer.
 * @param str_pool_cap     Capacity of @p str_pool in bytes.
 * @param str_offsets      Caller-owned array for string offsets.
 * @param str_offsets_count Number of elements in @p str_offsets.
 * @param err              Optional parse error info (line/message) on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_* on failure.
 */
cfgpack_err_t cfgpack_parse_schema(const char *data,
                                   size_t data_len,
                                   cfgpack_schema_t *out_schema,
                                   cfgpack_entry_t *entries,
                                   size_t max_entries,
                                   cfgpack_value_t *values,
                                   char *str_pool,
                                   size_t str_pool_cap,
                                   uint16_t *str_offsets,
                                   size_t str_offsets_count,
                                   cfgpack_parse_error_t *err);

/**
 * @brief Free schema resources (no-op for caller-owned buffers).
 * @param schema Schema to free.
 */
void cfgpack_schema_free(cfgpack_schema_t *schema);

/* Forward declaration - full definition in api.h */
typedef struct cfgpack_ctx cfgpack_ctx_t;

/**
 * @brief Write a schema and its current values to a JSON buffer.
 *
 * Output format (pretty-printed with 2-space indentation):
 * {
 *   "name": "demo",
 *   "version": 1,
 *   "entries": [
 *     {"index": 0, "name": "foo", "type": "u8", "value": 255},
 *     {"index": 1, "name": "bar", "type": "str", "value": "hello"},
 *     {"index": 2, "name": "baz", "type": "str", "value": null}
 *   ]
 * }
 *
 * - `value` is `null` for entries with has_default == 0 (NIL).
 * - Strings are JSON-escaped.
 * - Numbers are output as JSON numbers.
 *
 * @param ctx      Initialized context containing schema and values.
 * @param out      Output buffer for JSON.
 * @param out_cap  Capacity of @p out in bytes.
 * @param out_len  Output: bytes written (always set, even if > out_cap to indicate needed size).
 * @param err      Optional error info on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_BOUNDS if buffer too small.
 */
cfgpack_err_t cfgpack_schema_write_json(const cfgpack_ctx_t *ctx,
                                        char *out,
                                        size_t out_cap,
                                        size_t *out_len,
                                        cfgpack_parse_error_t *err);

/**
 * @brief Parse a schema from a JSON buffer.
 *
 * Expects the same JSON format produced by cfgpack_schema_write_json().
 * Entries are sorted by index after parsing.
 *
 * Default values are written directly into @p values and @p str_pool.
 *
 * @param data             Input buffer containing JSON data.
 * @param data_len         Length of data in bytes.
 * @param out_schema       Filled on success; points at caller-owned entries array.
 * @param entries          Caller-owned array to store parsed entries.
 * @param max_entries      Capacity of @p entries.
 * @param values           Caller-owned value slots to receive defaults (>= max_entries).
 * @param str_pool         Caller-owned string pool buffer.
 * @param str_pool_cap     Capacity of @p str_pool in bytes.
 * @param str_offsets      Caller-owned array for string offsets.
 * @param str_offsets_count Number of elements in @p str_offsets.
 * @param err              Optional parse error info (line/message) on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_* on failure.
 */
cfgpack_err_t cfgpack_schema_parse_json(const char *data,
                                        size_t data_len,
                                        cfgpack_schema_t *out_schema,
                                        cfgpack_entry_t *entries,
                                        size_t max_entries,
                                        cfgpack_value_t *values,
                                        char *str_pool,
                                        size_t str_pool_cap,
                                        uint16_t *str_offsets,
                                        size_t str_offsets_count,
                                        cfgpack_parse_error_t *err);

#endif /* CFGPACK_SCHEMA_H */
