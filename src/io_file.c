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
 * @brief Read an entire file into a scratch buffer.
 *
 * In text mode one byte of scratch is reserved for a NUL terminator.
 * A file that fills the buffer exactly is accepted: feof() is only set once
 * a read runs past the end, so after a full read one more byte is probed.
 *
 * @param path        File path to read.
 * @param scratch     Scratch buffer to hold file contents.
 * @param scratch_cap Capacity of scratch buffer.
 * @param binary      Non-zero to open in binary mode without a terminator.
 * @param out_len     Output: bytes read.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ARGS on NULL path or scratch;
 *         CFGPACK_ERR_IO on open/read error or if the file does not fit.
 */
static cfgpack_err_t read_file_impl(const char *path,
                                    void *scratch,
                                    size_t scratch_cap,
                                    int binary,
                                    size_t *out_len) {
    size_t want;
    size_t n;
    FILE *f;

    if (!path || !scratch || scratch_cap == 0) {
        return (CFGPACK_ERR_ARGS);
    }
    want = binary ? scratch_cap : scratch_cap - 1;

    f = fopen(path, binary ? "rb" : "r");
    if (!f) {
        return (CFGPACK_ERR_IO);
    }

    n = fread(scratch, 1, want, f);
    if (ferror(f)) {
        fclose(f);
        return (CFGPACK_ERR_IO);
    }
    if (n == want && fgetc(f) != EOF) {
        fclose(f);
        return (CFGPACK_ERR_IO); /* file too big for scratch */
    }
    fclose(f);

    if (!binary) {
        ((char *)scratch)[n] = '\0';
    }
    if (out_len) {
        *out_len = n;
    }
    return (CFGPACK_OK);
}

static cfgpack_err_t read_file(const char *path,
                               char *scratch,
                               size_t scratch_cap,
                               size_t *out_len) {
    return (read_file_impl(path, scratch, scratch_cap, 0, out_len));
}

static cfgpack_err_t read_file_binary(const char *path,
                                      uint8_t *scratch,
                                      size_t scratch_cap,
                                      size_t *out_len) {
    return (read_file_impl(path, scratch, scratch_cap, 1, out_len));
}

/**
 * @brief Write a buffer to a file.
 *
 * Buffered write errors (ENOSPC, EIO) commonly surface only at fflush or
 * fclose, so both results are checked before reporting success.
 *
 * @param path   File path to write.
 * @param data   Data to write.
 * @param len    Length of data.
 * @param binary Non-zero to open in binary mode.
 * @return CFGPACK_OK on success, CFGPACK_ERR_IO on failure.
 */
static cfgpack_err_t write_file_impl(const char *path,
                                     const void *data,
                                     size_t len,
                                     int binary) {
    FILE *f;

    if (!path || (!data && len > 0)) {
        return (CFGPACK_ERR_ARGS);
    }
    f = fopen(path, binary ? "wb" : "w");
    if (!f) {
        return (CFGPACK_ERR_IO);
    }

    if (fwrite(data, 1, len, f) != len || fflush(f) != 0) {
        fclose(f);
        return (CFGPACK_ERR_IO);
    }
    if (fclose(f) != 0) {
        return (CFGPACK_ERR_IO);
    }
    return (CFGPACK_OK);
}

static cfgpack_err_t write_file(const char *path,
                                const char *data,
                                size_t len) {
    return (write_file_impl(path, data, len, 0));
}

static cfgpack_err_t write_file_binary(const char *path,
                                       const uint8_t *data,
                                       size_t len) {
    return (write_file_impl(path, data, len, 1));
}

cfgpack_err_t cfgpack_schema_measure_file(const char *path,
                                          cfgpack_schema_measure_t *out,
                                          char *scratch,
                                          size_t scratch_cap,
                                          cfgpack_parse_error_t *err) {
    cfgpack_err_t rc;
    size_t len = 0;

    rc = read_file(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        if (err) {
            err->line = 0;
            err->code = CFGPACK_PARSE_FILE_OPEN;
        }
        return (rc);
    }
    return (cfgpack_schema_measure(scratch, len, out, err));
}

cfgpack_err_t cfgpack_schema_measure_json_file(const char *path,
                                               cfgpack_schema_measure_t *out,
                                               char *scratch,
                                               size_t scratch_cap,
                                               cfgpack_parse_error_t *err) {
    cfgpack_err_t rc;
    size_t len = 0;

    rc = read_file(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        if (err) {
            err->line = 0;
            err->code = CFGPACK_PARSE_FILE_OPEN;
        }
        return (rc);
    }
    return (cfgpack_schema_measure_json(scratch, len, out, err));
}

cfgpack_err_t cfgpack_parse_schema_file(const char *path,
                                        const cfgpack_parse_opts_t *opts,
                                        char *scratch,
                                        size_t scratch_cap) {
    cfgpack_err_t rc;
    size_t len = 0;

    if (!opts) {
        return (CFGPACK_ERR_ARGS);
    }
    rc = read_file(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        if (opts->err) {
            opts->err->line = 0;
            opts->err->code = CFGPACK_PARSE_FILE_OPEN;
        }
        return (rc);
    }
    return (cfgpack_parse_schema(scratch, len, opts));
}

cfgpack_err_t cfgpack_schema_parse_json_file(const char *path,
                                             const cfgpack_parse_opts_t *opts,
                                             char *scratch,
                                             size_t scratch_cap) {
    cfgpack_err_t rc;
    size_t len = 0;

    if (!opts) {
        return (CFGPACK_ERR_ARGS);
    }
    rc = read_file(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        if (opts->err) {
            opts->err->line = 0;
            opts->err->code = CFGPACK_PARSE_FILE_OPEN;
        }
        return (rc);
    }
    return (cfgpack_schema_parse_json(scratch, len, opts));
}

cfgpack_err_t cfgpack_schema_write_json_file(const cfgpack_ctx_t *ctx,
                                             const char *path,
                                             char *scratch,
                                             size_t scratch_cap,
                                             cfgpack_parse_error_t *err) {
    size_t out_len = 0;
    cfgpack_err_t rc;

    rc = cfgpack_schema_write_json(ctx, scratch, scratch_cap, &out_len, err);
    if (rc != CFGPACK_OK) {
        return (rc);
    }

    if (out_len > scratch_cap) {
        if (err) {
            err->line = 0;
            err->code = CFGPACK_PARSE_SCRATCH_TOO_SMALL;
        }
        return (CFGPACK_ERR_BOUNDS);
    }

    return (write_file(path, scratch, out_len));
}

cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx,
                                   const char *path,
                                   uint8_t *scratch,
                                   size_t scratch_cap) {
    cfgpack_err_t rc;
    size_t len = 0;

    rc = cfgpack_pageout(ctx, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        return (rc);
    }

    return (write_file_binary(path, scratch, len));
}

cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx,
                                  const char *path,
                                  uint8_t *scratch,
                                  size_t scratch_cap) {
    cfgpack_err_t rc;
    size_t len = 0;

    rc = read_file_binary(path, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) {
        return (rc);
    }

    return (cfgpack_pagein_buf(ctx, scratch, len));
}
