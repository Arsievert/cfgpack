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

#ifndef CFGPACK_HOSTED
  #define CFGPACK_EMBEDDED 1
#endif

#ifdef CFGPACK_HOSTED
  #include <stdio.h>
  #define CFGPACK_PRINTF(...) printf(__VA_ARGS__)
#else
  #define CFGPACK_PRINTF(...) ((void)0)
#endif

#endif /* CFGPACK_CONFIG_H */
