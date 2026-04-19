#ifndef CFGPACK_IO_LITTLEFS_H
#define CFGPACK_IO_LITTLEFS_H

/**
 * @file io_littlefs.h
 * @brief Optional LittleFS-based convenience wrappers for cfgpack.
 *
 * These functions use LittleFS operations and are provided for embedded
 * systems with flash storage. The caller owns the lfs_t instance and
 * must mount/unmount it externally.
 *
 * The scratch buffer serves double duty: the first cfg->cache_size bytes
 * are used as the LittleFS file cache (via lfs_file_opencfg), and the
 * remainder holds the serialized data. This avoids lfs_malloc, making
 * the wrapper compatible with LFS_NO_MALLOC builds.
 *
 * To use these functions, compile with -DCFGPACK_LITTLEFS and link
 * src/io_littlefs.c and the LittleFS sources with your project.
 */

#ifdef CFGPACK_LITTLEFS

  #include "api.h"
  #include "lfs.h"

/**
 * @brief Encode to a LittleFS file using caller scratch buffer (no heap).
 *
 * Serializes the context into a MessagePack blob via cfgpack_pageout(),
 * then writes it to a LittleFS file. The caller owns the lfs_t instance
 * and must have it mounted before calling.
 *
 * @param ctx          Initialized context.
 * @param lfs          Mounted LittleFS instance (caller-owned).
 * @param path         Destination file path within LittleFS.
 * @param scratch      Scratch buffer (must be >= cfg->cache_size + encoded size).
 * @param scratch_cap  Capacity of @p scratch in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if data portion too small;
 *         CFGPACK_ERR_BOUNDS if scratch < cache_size;
 *         CFGPACK_ERR_IO on LittleFS write failures.
 */
cfgpack_err_t cfgpack_pageout_lfs(const cfgpack_ctx_t *ctx,
                                  lfs_t *lfs,
                                  const char *path,
                                  uint8_t *scratch,
                                  size_t scratch_cap);

/**
 * @brief Decode from a LittleFS file using caller scratch buffer (no heap).
 *
 * Reads the file contents into the scratch buffer, then calls
 * cfgpack_pagein_buf() to deserialize the MessagePack payload.
 * The caller owns the lfs_t instance and must have it mounted.
 *
 * @param ctx          Initialized context.
 * @param lfs          Mounted LittleFS instance (caller-owned).
 * @param path         Source file path within LittleFS.
 * @param scratch      Scratch buffer (must be >= cfg->cache_size + file size).
 * @param scratch_cap  Capacity of @p scratch.
 * @return CFGPACK_OK on success; CFGPACK_ERR_BOUNDS if scratch < cache_size;
 *         CFGPACK_ERR_IO on read/size errors;
 *         CFGPACK_ERR_DECODE if payload is invalid.
 */
cfgpack_err_t cfgpack_pagein_lfs(cfgpack_ctx_t *ctx,
                                 lfs_t *lfs,
                                 const char *path,
                                 uint8_t *scratch,
                                 size_t scratch_cap);

#endif /* CFGPACK_LITTLEFS */
#endif /* CFGPACK_IO_LITTLEFS_H */
