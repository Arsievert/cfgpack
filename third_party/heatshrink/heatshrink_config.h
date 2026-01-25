#ifndef HEATSHRINK_CONFIG_H
#define HEATSHRINK_CONFIG_H

/* CFGPack configuration: static allocation for embedded use */

/* Disable dynamic allocation - use static buffers only */
#define HEATSHRINK_DYNAMIC_ALLOC 0

/* Static configuration for decoder */
#define HEATSHRINK_STATIC_INPUT_BUFFER_SIZE 64
#define HEATSHRINK_STATIC_WINDOW_BITS 8      /* 256-byte history window */
#define HEATSHRINK_STATIC_LOOKAHEAD_BITS 4   /* 16-byte lookahead */

/* Disable debugging logs */
#define HEATSHRINK_DEBUGGING_LOGS 0

/* Index improves encoder performance (not needed for decoder-only) */
#define HEATSHRINK_USE_INDEX 1

#endif
