/**
 * @file config.h
 * @brief Build configuration for cfgpack library.
 *
 * Build modes:
 *   CFGPACK_EMBEDDED (default) - No stdio dependency in library
 *   CFGPACK_HOSTED             - Full printf/snprintf support (define to enable)
 *
 * In embedded mode:
 *   - cfgpack_print() / cfgpack_print_all() are silent no-ops (return CFGPACK_OK)
 *   - Float formatting uses minimal implementation (no snprintf)
 *   - No <stdio.h> dependency in core library
 *
 * To enable hosted mode, compile with -DCFGPACK_HOSTED
 */
#ifndef CFGPACK_CONFIG_H
#define CFGPACK_CONFIG_H

#include <limits.h> /* CHAR_BIT */

#ifndef CFGPACK_HOSTED
  #define CFGPACK_EMBEDDED 1
#endif

#ifdef CFGPACK_HOSTED
  #include <stdio.h>
  #define CFGPACK_PRINTF(...) printf(__VA_ARGS__)
#else
  #define CFGPACK_PRINTF(...) ((void)0)
#endif

/**
 * @brief Maximum number of schema entries supported.
 *
 * This determines the size of the inline presence bitmap in cfgpack_ctx_t.
 * Override by defining CFGPACK_MAX_ENTRIES before including cfgpack headers.
 */
#ifndef CFGPACK_MAX_ENTRIES
  #define CFGPACK_MAX_ENTRIES 128
#endif

/**
 * @brief Size in bytes of the presence bitmap.
 */
#define CFGPACK_PRESENCE_BYTES ((CFGPACK_MAX_ENTRIES + CHAR_BIT - 1) / CHAR_BIT)

/**
 * @brief Maximum nesting depth for cfgpack_msgpack_skip_value().
 *
 * Limits the depth of nested msgpack maps/arrays that can be skipped.
 * Each level costs 8 bytes of stack (32 levels = 256 bytes).
 * Override by defining CFGPACK_SKIP_MAX_DEPTH before including cfgpack headers.
 */
#ifndef CFGPACK_SKIP_MAX_DEPTH
  #define CFGPACK_SKIP_MAX_DEPTH 32
#endif

#endif /* CFGPACK_CONFIG_H */
