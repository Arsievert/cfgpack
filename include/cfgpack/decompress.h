#ifndef CFGPACK_DECOMPRESS_H
#define CFGPACK_DECOMPRESS_H

/**
 * @file decompress.h
 * @brief Optional decompression support for cfgpack.
 *
 * Provides LZ4 and heatshrink decompression wrappers that decompress data
 * into a caller-provided scratch buffer, then call cfgpack_pagein_buf().
 *
 * Enable with compile flags:
 * - CFGPACK_LZ4: Enable LZ4 decompression
 * - CFGPACK_HEATSHRINK: Enable heatshrink decompression
 *
 * Note: The heatshrink path uses a static decoder instance and is NOT
 * thread-safe. The LZ4 path is fully reentrant.
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
 * Decompresses the LZ4-compressed data into the caller-provided scratch
 * buffer, then calls cfgpack_pagein_buf() to parse the MessagePack payload.
 *
 * @param ctx               Initialized cfgpack context.
 * @param data              LZ4-compressed data.
 * @param len               Length of compressed data in bytes.
 * @param decompressed_size Expected size of decompressed data (must be known).
 * @param scratch           Caller-provided buffer for decompressed output.
 * @param scratch_cap       Capacity of scratch buffer in bytes.
 * @return CFGPACK_OK on success;
 *         CFGPACK_ERR_BOUNDS if decompressed_size > scratch_cap;
 *         CFGPACK_ERR_DECODE on decompression failure or NULL arguments;
 *         Other errors from cfgpack_pagein_buf().
 */
cfgpack_err_t cfgpack_pagein_lz4(cfgpack_ctx_t *ctx,
                                 const uint8_t *data,
                                 size_t len,
                                 size_t decompressed_size,
                                 uint8_t *scratch,
                                 size_t scratch_cap);
#endif /* CFGPACK_LZ4 */

#ifdef CFGPACK_HEATSHRINK
/**
 * @brief Decompress heatshrink data and load into context.
 *
 * Decompresses the heatshrink-compressed data into the caller-provided
 * scratch buffer, then calls cfgpack_pagein_buf() to parse the MessagePack
 * payload.
 *
 * The encoder must use matching parameters:
 * - Window size: 8 bits (256 bytes)
 * - Lookahead: 4 bits (16 bytes)
 *
 * @param ctx         Initialized cfgpack context.
 * @param data        Heatshrink-compressed data.
 * @param len         Length of compressed data in bytes.
 * @param scratch     Caller-provided buffer for decompressed output.
 * @param scratch_cap Capacity of scratch buffer in bytes.
 * @return CFGPACK_OK on success;
 *         CFGPACK_ERR_BOUNDS if decompressed data exceeds scratch_cap;
 *         CFGPACK_ERR_DECODE on decompression failure or NULL arguments;
 *         Other errors from cfgpack_pagein_buf().
 */
cfgpack_err_t cfgpack_pagein_heatshrink(cfgpack_ctx_t *ctx,
                                        const uint8_t *data,
                                        size_t len,
                                        uint8_t *scratch,
                                        size_t scratch_cap);
#endif /* CFGPACK_HEATSHRINK */

#endif /* CFGPACK_DECOMPRESS_H */
