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
 * Default values are written directly into opts->values and opts->str_pool.
 *
 * @param path        File path to read.
 * @param opts        Parse options containing output buffers and error pointer.
 * @param scratch     Scratch buffer to hold file contents.
 * @param scratch_cap Capacity of scratch buffer.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read error; CFGPACK_ERR_* on parse error.
 */
cfgpack_err_t cfgpack_parse_schema_file(const char *path,
                                        const cfgpack_parse_opts_t *opts,
                                        char *scratch,
                                        size_t scratch_cap);

/**
 * @brief Parse a JSON schema from a file.
 *
 * Reads the file into a scratch buffer and calls cfgpack_schema_parse_json().
 * Default values are written directly into opts->values and opts->str_pool.
 *
 * @param path        File path to read.
 * @param opts        Parse options containing output buffers and error pointer.
 * @param scratch     Scratch buffer to hold file contents.
 * @param scratch_cap Capacity of scratch buffer.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read error; CFGPACK_ERR_* on parse error.
 */
cfgpack_err_t cfgpack_schema_parse_json_file(const char *path,
                                             const cfgpack_parse_opts_t *opts,
                                             char *scratch,
                                             size_t scratch_cap);

/**
 * @brief Measure buffer requirements for a .map schema file.
 *
 * Reads the file into a scratch buffer and calls cfgpack_schema_measure().
 *
 * @param path        File path to read.
 * @param out         Filled with measurement results.
 * @param scratch     Scratch buffer to hold file contents.
 * @param scratch_cap Capacity of scratch buffer.
 * @param err         Optional parse error info on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read error; CFGPACK_ERR_* on parse error.
 */
cfgpack_err_t cfgpack_schema_measure_file(const char *path,
                                          cfgpack_schema_measure_t *out,
                                          char *scratch,
                                          size_t scratch_cap,
                                          cfgpack_parse_error_t *err);

/**
 * @brief Measure buffer requirements for a JSON schema file.
 *
 * Reads the file into a scratch buffer and calls cfgpack_schema_measure_json().
 *
 * @param path        File path to read.
 * @param out         Filled with measurement results.
 * @param scratch     Scratch buffer to hold file contents.
 * @param scratch_cap Capacity of scratch buffer.
 * @param err         Optional parse error info on failure.
 * @return CFGPACK_OK on success; CFGPACK_ERR_IO on read error; CFGPACK_ERR_* on parse error.
 */
cfgpack_err_t cfgpack_schema_measure_json_file(const char *path,
                                               cfgpack_schema_measure_t *out,
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
cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx,
                                   const char *path,
                                   uint8_t *scratch,
                                   size_t scratch_cap);

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
cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx,
                                  const char *path,
                                  uint8_t *scratch,
                                  size_t scratch_cap);

#endif /* CFGPACK_IO_FILE_H */
