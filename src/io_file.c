/**
 * @file io_file.c
 * @brief Optional file-based convenience wrappers for cfgpack.
 *
 * These functions use FILE* operations and are provided for convenience
 * on desktop/POSIX systems. For embedded systems without file I/O,
 * use the buffer-based functions in api.h and schema.h instead.
 */

#include "cfgpack/io_file.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Read entire file into scratch buffer.
 * @param path        File path to read.
 * @param scratch     Scratch buffer to hold file contents.
 * @param scratch_cap Capacity of scratch buffer.
 * @param out_len     Output: bytes read.
 * @return CFGPACK_OK on success, CFGPACK_ERR_IO on failure.
 */
static cfgpack_err_t read_file(const char *path, char *scratch, size_t scratch_cap, size_t *out_len) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return CFGPACK_ERR_IO;
    }

    size_t n = fread(scratch, 1, scratch_cap - 1, f);
    if (!feof(f)) {
        fclose(f);
        return CFGPACK_ERR_IO; /* file too big for scratch */
    }
    fclose(f);

    scratch[n] = '\0';
    if (out_len) {
        *out_len = n;
    }
    return CFGPACK_OK;
}

/**
 * @brief Read entire file into scratch buffer (binary mode).
 * @param path        File path to read.
 * @param scratch     Scratch buffer to hold file contents.
 * @param scratch_cap Capacity of scratch buffer.
 * @param out_len     Output: bytes read.
 * @return CFGPACK_OK on success, CFGPACK_ERR_IO on failure.
 */
static cfgpack_err_t read_file_binary(const char *path, uint8_t *scratch, size_t scratch_cap, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return CFGPACK_ERR_IO;
    }

    size_t n = fread(scratch, 1, scratch_cap, f);
    if (n == scratch_cap && !feof(f)) {
        fclose(f);
        return CFGPACK_ERR_IO; /* file too big for scratch */
    }
    fclose(f);

    if (out_len) {
        *out_len = n;
    }
    return CFGPACK_OK;
}

/**
 * @brief Write buffer to file.
 * @param path File path to write.
 * @param data Data to write.
 * @param len  Length of data.
 * @return CFGPACK_OK on success, CFGPACK_ERR_IO on failure.
 */
static cfgpack_err_t write_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "w");
    if (!f) {
        return CFGPACK_ERR_IO;
    }

    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return CFGPACK_ERR_IO;
    }
    fclose(f);
    return CFGPACK_OK;
}

/**
 * @brief Write buffer to file (binary mode).
 * @param path File path to write.
 * @param data Data to write.
 * @param len  Length of data.
 * @return CFGPACK_OK on success, CFGPACK_ERR_IO on failure.
 */
static cfgpack_err_t write_file_binary(const char *path, const uint8_t *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return CFGPACK_ERR_IO;
    }

    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return CFGPACK_ERR_IO;
    }
    fclose(f);
    return CFGPACK_OK;
}

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
                                        cfgpack_parse_error_t *err) {
    size_t len = 0;
    cfgpack_err_t rc = read_file(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        if (err) {
            err->line = 0;
            snprintf(err->message, sizeof(err->message), "unable to open file");
        }
        return rc;
    }
    return cfgpack_parse_schema(scratch,
                                len,
                                out_schema,
                                entries,
                                max_entries,
                                values,
                                str_pool,
                                str_pool_cap,
                                str_offsets,
                                str_offsets_count,
                                err);
}

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
                                             cfgpack_parse_error_t *err) {
    size_t len = 0;
    cfgpack_err_t rc = read_file(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        if (err) {
            err->line = 0;
            snprintf(err->message, sizeof(err->message), "unable to open file");
        }
        return rc;
    }
    return cfgpack_schema_parse_json(scratch,
                                     len,
                                     out_schema,
                                     entries,
                                     max_entries,
                                     values,
                                     str_pool,
                                     str_pool_cap,
                                     str_offsets,
                                     str_offsets_count,
                                     err);
}

cfgpack_err_t cfgpack_schema_write_json_file(const cfgpack_ctx_t *ctx,
                                             const char *path,
                                             char *scratch,
                                             size_t scratch_cap,
                                             cfgpack_parse_error_t *err) {
    size_t out_len = 0;
    cfgpack_err_t rc = cfgpack_schema_write_json(ctx, scratch, scratch_cap, &out_len, err);
    if (rc != CFGPACK_OK) {
        return rc;
    }

    if (out_len > scratch_cap) {
        if (err) {
            err->line = 0;
            snprintf(err->message, sizeof(err->message), "scratch buffer too small");
        }
        return CFGPACK_ERR_BOUNDS;
    }

    return write_file(path, scratch, out_len);
}

cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap) {
    size_t len = 0;
    cfgpack_err_t rc = cfgpack_pageout(ctx, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        return rc;
    }

    return write_file_binary(path, scratch, len);
}

cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap) {
    size_t len = 0;
    cfgpack_err_t rc = read_file_binary(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        return rc;
    }

    return cfgpack_pagein_buf(ctx, scratch, len);
}
