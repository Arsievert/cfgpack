#ifndef CFGPACK_SCHEMA_H
#define CFGPACK_SCHEMA_H

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
 * @brief Parse a schema file into caller-provided buffers (no heap).
 *
 * Expected format:
 *   - Header: "<name> <version>" on the first non-comment line.
 *   - Entries: "<index> <name> <type> <default>" on subsequent non-comment lines.
 *   - Default values: NIL (no default), integer literals, float literals, or "quoted strings".
 * Comments (lines starting with '#') and blank lines are ignored.
 * Fails on duplicates (index or name), name length >5, unsupported type,
 * too many entries, or indices outside 0..65535.
 *
 * @param path        Schema file path to read.
 * @param out_schema  Filled on success; points at caller-owned entries array.
 * @param entries     Caller-owned array to store parsed entries.
 * @param max_entries Capacity of @p entries.
 * @param defaults    Caller-owned array to store default values (parallel to entries).
 * @param err         Optional parse error info (line/message) on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_* on failure.
 */
cfgpack_err_t cfgpack_parse_schema(const char *path, cfgpack_schema_t *out_schema, cfgpack_entry_t *entries, size_t max_entries, cfgpack_value_t *defaults, cfgpack_parse_error_t *err);

/**
 * @brief Free schema resources (no-op for caller-owned buffers).
 * @param schema Schema to free.
 */
void cfgpack_schema_free(cfgpack_schema_t *schema);

/**
 * @brief Write a simple Markdown table describing the schema.
 *
 * @param schema   Schema to describe.
 * @param defaults Parallel array of default values (same order as schema->entries).
 * @param out_path Output file path for Markdown.
 * @param err      Optional error info on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on write failure.
 */
cfgpack_err_t cfgpack_schema_write_markdown(const cfgpack_schema_t *schema, const cfgpack_value_t *defaults, const char *out_path, cfgpack_parse_error_t *err);

#endif /* CFGPACK_SCHEMA_H */
