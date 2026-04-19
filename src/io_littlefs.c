/**
 * @file io_littlefs.c
 * @brief Optional LittleFS-based convenience wrappers for cfgpack.
 *
 * These functions use LittleFS operations and are provided for embedded
 * systems with flash storage. The caller owns the lfs_t instance and
 * must mount/unmount it externally.
 *
 * To use these functions, compile with -DCFGPACK_LITTLEFS and link
 * src/io_littlefs.c and the LittleFS sources with your project.
 */

#ifdef CFGPACK_LITTLEFS

  #include "cfgpack/io_littlefs.h"

  #include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Scratch buffer layout helpers
 * ───────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Split scratch into file-cache and data regions.
 *
 * The first cache_size bytes serve as the LittleFS file cache
 * (passed to lfs_file_opencfg), the remainder is the data buffer.
 */
static cfgpack_err_t split_scratch(const lfs_t *lfs,
                                   uint8_t *scratch,
                                   size_t scratch_cap,
                                   uint8_t **file_cache,
                                   uint8_t **data_buf,
                                   size_t *data_cap) {
    lfs_size_t cache_size = lfs->cfg->cache_size;

    if (scratch_cap <= cache_size) {
        return (CFGPACK_ERR_BOUNDS);
    }

    *file_cache = scratch;
    *data_buf = scratch + cache_size;
    *data_cap = scratch_cap - cache_size;
    return (CFGPACK_OK);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal file helpers
 * ───────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Read entire LittleFS file into buffer.
 */
static cfgpack_err_t read_lfs_file(lfs_t *lfs,
                                   const char *path,
                                   uint8_t *file_cache,
                                   uint8_t *buf,
                                   size_t buf_cap,
                                   size_t *out_len) {
    struct lfs_file_config file_cfg;
    lfs_ssize_t size;
    lfs_ssize_t n;
    lfs_file_t file;
    int err;

    memset(&file_cfg, 0, sizeof(file_cfg));
    file_cfg.buffer = file_cache;

    err = lfs_file_opencfg(lfs, &file, path, LFS_O_RDONLY, &file_cfg);
    if (err < 0) {
        return (CFGPACK_ERR_IO);
    }

    size = lfs_file_size(lfs, &file);
    if (size < 0 || (size_t)size > buf_cap) {
        lfs_file_close(lfs, &file);
        return (CFGPACK_ERR_IO);
    }

    n = lfs_file_read(lfs, &file, buf, (lfs_size_t)size);
    lfs_file_close(lfs, &file);

    if (n != size) {
        return (CFGPACK_ERR_IO);
    }

    *out_len = (size_t)n;
    return (CFGPACK_OK);
}

/**
 * @brief Write buffer to a LittleFS file.
 */
static cfgpack_err_t write_lfs_file(lfs_t *lfs,
                                    const char *path,
                                    uint8_t *file_cache,
                                    const uint8_t *data,
                                    size_t len) {
    struct lfs_file_config file_cfg;
    lfs_ssize_t n;
    lfs_file_t file;
    int err;

    memset(&file_cfg, 0, sizeof(file_cfg));
    file_cfg.buffer = file_cache;

    err = lfs_file_opencfg(lfs, &file, path,
                           LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &file_cfg);
    if (err < 0) {
        return (CFGPACK_ERR_IO);
    }

    n = lfs_file_write(lfs, &file, data, (lfs_size_t)len);
    lfs_file_close(lfs, &file);

    if (n < 0 || (size_t)n != len) {
        return (CFGPACK_ERR_IO);
    }
    return (CFGPACK_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

cfgpack_err_t cfgpack_pageout_lfs(const cfgpack_ctx_t *ctx,
                                  lfs_t *lfs,
                                  const char *path,
                                  uint8_t *scratch,
                                  size_t scratch_cap) {
    uint8_t *file_cache;
    uint8_t *data_buf;
    cfgpack_err_t rc;
    size_t data_cap;
    size_t len = 0;

    rc = split_scratch(lfs, scratch, scratch_cap, &file_cache, &data_buf,
                       &data_cap);
    if (rc != CFGPACK_OK) {
        return (rc);
    }

    rc = cfgpack_pageout(ctx, data_buf, data_cap, &len);
    if (rc != CFGPACK_OK) {
        return (rc);
    }

    return (write_lfs_file(lfs, path, file_cache, data_buf, len));
}

cfgpack_err_t cfgpack_pagein_lfs(cfgpack_ctx_t *ctx,
                                 lfs_t *lfs,
                                 const char *path,
                                 uint8_t *scratch,
                                 size_t scratch_cap) {
    uint8_t *file_cache;
    uint8_t *data_buf;
    cfgpack_err_t rc;
    size_t data_cap;
    size_t len = 0;

    rc = split_scratch(lfs, scratch, scratch_cap, &file_cache, &data_buf,
                       &data_cap);
    if (rc != CFGPACK_OK) {
        return (rc);
    }

    rc = read_lfs_file(lfs, path, file_cache, data_buf, data_cap, &len);
    if (rc != CFGPACK_OK) {
        return (rc);
    }

    return (cfgpack_pagein_buf(ctx, data_buf, len));
}

#endif /* CFGPACK_LITTLEFS */
