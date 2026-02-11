#ifndef CFGPACK_IO_FILE_H
#define CFGPACK_IO_FILE_H

/**
 * @file io_file.h
 * @brief Optional file-based convenience wrappers for cfgpack.
 *
 * These functions use FILE* operations and are provided for convenience
 * on desktop/POSIX systems. For embedded systems without file I/O,
 * use the buffer-based functions in api.h and schema.h instead.
 *
 * To use these functions, compile and link src/io_file.c with your project.
 */

#include "api.h"
#include "schema.h"

/**
 * @brief Parse a .map schema from a file.
 *
 * Reads the file into a scratch buffer and calls cfgpack_parse_schema().
 * Default values are written directly into @p values and @p str_pool.
 *
 * @param path             File path to read.
 * @param out_schema       Filled on success; points at caller-owned entries array.
 * @param entries          Caller-owned array to store parsed entries.
 * @param max_entries      Capacity of @p entries.
 * @param values           Caller-owned value slots to receive defaults.
 * @param str_pool         Caller-owned string pool buffer.
 * @param str_pool_cap     Capacity of @p str_pool in bytes.
 * @param str_offsets      Caller-owned array for string offsets.
 * @param str_offsets_count Number of elements in @p str_offsets.
 * @param scratch          Scratch buffer to hold file contents.
 * @param scratch_cap      Capacity of scratch buffer.
 * @param err              Optional parse error info (line/message) on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read error; CFGPACK_ERR_* on parse error.
 */
cfgpack_err_t cfgpack_parse_schema_file(const char *path,
                                        cfgpack_schema_t *out_schema,
                                        cfgpack_entry_t *entries,
                                        size_t max_entries,
                                        cfgpack_value_t *values,
                                        char *str_pool,
                                        size_t str_pool_cap,
                                        uint16_t *str_offsets,
                                        size_t str_offsets_count,
                                        char *scratch,
                                        size_t scratch_cap,
                                        cfgpack_parse_error_t *err);

/**
 * @brief Parse a JSON schema from a file.
 *
 * Reads the file into a scratch buffer and calls cfgpack_schema_parse_json().
 * Default values are written directly into @p values and @p str_pool.
 *
 * @param path             File path to read.
 * @param out_schema       Filled on success; points at caller-owned entries array.
 * @param entries          Caller-owned array to store parsed entries.
 * @param max_entries      Capacity of @p entries.
 * @param values           Caller-owned value slots to receive defaults.
 * @param str_pool         Caller-owned string pool buffer.
 * @param str_pool_cap     Capacity of @p str_pool in bytes.
 * @param str_offsets      Caller-owned array for string offsets.
 * @param str_offsets_count Number of elements in @p str_offsets.
 * @param scratch          Scratch buffer to hold file contents.
 * @param scratch_cap      Capacity of scratch buffer.
 * @param err              Optional parse error info (line/message) on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read error; CFGPACK_ERR_* on parse error.
 */
cfgpack_err_t cfgpack_schema_parse_json_file(const char *path,
                                             cfgpack_schema_t *out_schema,
                                             cfgpack_entry_t *entries,
                                             size_t max_entries,
                                             cfgpack_value_t *values,
                                             char *str_pool,
                                             size_t str_pool_cap,
                                             uint16_t *str_offsets,
                                             size_t str_offsets_count,
                                             char *scratch,
                                             size_t scratch_cap,
                                             cfgpack_parse_error_t *err);

/**
 * @brief Write schema and values as JSON to a file.
 *
 * Writes to a scratch buffer and then writes the buffer to the file.
 *
 * @param ctx         Initialized context containing schema and values.
 * @param path        Output file path.
 * @param scratch     Scratch buffer for formatting.
 * @param scratch_cap Capacity of scratch buffer.
 * @param err         Optional error info on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on write failure; CFGPACK_ERR_BOUNDS if scratch too small.
 */
cfgpack_err_t cfgpack_schema_write_json_file(const cfgpack_ctx_t *ctx,
                                             const char *path,
                                             char *scratch,
                                             size_t scratch_cap,
                                             cfgpack_parse_error_t *err);

/**
 * @brief Encode to a file using caller scratch buffer (no heap).
 *
 * @param ctx          Initialized context.
 * @param path         Destination file path.
 * @param scratch      Scratch buffer used for encode.
 * @param scratch_cap  Capacity of @p scratch in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if scratch too small;
 *         CFGPACK_ERR_IO on write failures.
 */
cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);

/**
 * @brief Decode from a file using caller scratch buffer (no heap).
 *
 * @param ctx          Initialized context.
 * @param path         Source file path.
 * @param scratch      Scratch buffer to hold file contents.
 * @param scratch_cap  Capacity of @p scratch.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read/size errors;
 *         CFGPACK_ERR_DECODE if payload is invalid.
 */
cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);

#endif /* CFGPACK_IO_FILE_H */
