#ifndef CFGPACK_DECOMPRESS_H
#define CFGPACK_DECOMPRESS_H

/**
 * @file decompress.h
 * @brief Optional decompression support for cfgpack.
 *
 * Provides LZ4 and heatshrink decompression wrappers that decompress data
 * into an internal static buffer, then call cfgpack_pagein_buf().
 *
 * Enable with compile flags:
 * - CFGPACK_LZ4: Enable LZ4 decompression
 * - CFGPACK_HEATSHRINK: Enable heatshrink decompression
 *
 * Note: These functions are NOT thread-safe due to the shared static buffer.
 * Compression must be done externally; only decompression is supported.
 */

#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "api.h"

#ifdef CFGPACK_LZ4
/**
 * @brief Decompress LZ4 data and load into context.
 *
 * Decompresses the LZ4-compressed data into an internal static buffer,
 * then calls cfgpack_pagein_buf() to parse the MessagePack payload.
 *
 * @param ctx               Initialized cfgpack context.
 * @param data              LZ4-compressed data.
 * @param len               Length of compressed data in bytes.
 * @param decompressed_size Expected size of decompressed data (must be known).
 * @return CFGPACK_OK on success;
 *         CFGPACK_ERR_BOUNDS if decompressed_size > 4096;
 *         CFGPACK_ERR_DECODE on decompression failure;
 *         Other errors from cfgpack_pagein_buf().
 */
cfgpack_err_t cfgpack_pagein_lz4(cfgpack_ctx_t *ctx,
                                 const uint8_t *data,
                                 size_t len,
                                 size_t decompressed_size);
#endif /* CFGPACK_LZ4 */

#ifdef CFGPACK_HEATSHRINK
/**
 * @brief Decompress heatshrink data and load into context.
 *
 * Decompresses the heatshrink-compressed data into an internal static buffer,
 * then calls cfgpack_pagein_buf() to parse the MessagePack payload.
 *
 * The encoder must use matching parameters:
 * - Window size: 8 bits (256 bytes)
 * - Lookahead: 4 bits (16 bytes)
 *
 * @param ctx  Initialized cfgpack context.
 * @param data Heatshrink-compressed data.
 * @param len  Length of compressed data in bytes.
 * @return CFGPACK_OK on success;
 *         CFGPACK_ERR_BOUNDS if decompressed data exceeds 4096 bytes;
 *         CFGPACK_ERR_DECODE on decompression failure;
 *         Other errors from cfgpack_pagein_buf().
 */
cfgpack_err_t cfgpack_pagein_heatshrink(cfgpack_ctx_t *ctx,
                                        const uint8_t *data,
                                        size_t len);
#endif /* CFGPACK_HEATSHRINK */

#endif /* CFGPACK_DECOMPRESS_H */
