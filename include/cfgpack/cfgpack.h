#ifndef CFGPACK_CFGPACK_H
#define CFGPACK_CFGPACK_H
/* IWYU pragma: always_keep */

/**
 * @file cfgpack.h
 * @brief Umbrella header for the cfgpack public API.
 *
 * Include this single header to access all cfgpack functionality:
 * - Error codes (error.h)
 * - Value types and containers (value.h)
 * - Schema parsing and serialization (schema.h)
 * - Runtime context and value access (api.h)
 *
 * For file-based convenience wrappers, also include io_file.h.
 * For LittleFS storage wrappers, also include io_littlefs.h.
 * For decompression support (LZ4/heatshrink), also include decompress.h.
 */
#include "api.h"
#include "config.h"
#include "decompress.h"
#include "error.h"
#include "schema.h"
#include "value.h"

#endif /* CFGPACK_CFGPACK_H */
