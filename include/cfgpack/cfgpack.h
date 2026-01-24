#ifndef CFGPACK_CFGPACK_H
#define CFGPACK_CFGPACK_H

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
 */
#include "error.h"
#include "value.h"
#include "schema.h"
#include "api.h"

#endif /* CFGPACK_CFGPACK_H */
