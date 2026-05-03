/**
 * @file crc32.h
 * @brief CRC-32C (Castagnoli) for cfgpack blob integrity.
 */
#ifndef CFGPACK_CRC32_H
#define CFGPACK_CRC32_H

#include <stddef.h>
#include <stdint.h>

#define CFGPACK_CRC_SIZE 4

/**
 * @brief Compute CRC-32C (Castagnoli) over a byte buffer.
 *
 * Uses the reflected polynomial 0x82F63B78 with a nibble-at-a-time table
 * (16 entries, 64 bytes).  Achieves HD=6 for data words up to ~8 KB.
 *
 * @param data  Input bytes (may be NULL when @p len is 0).
 * @param len   Number of bytes to checksum.
 * @return CRC-32C value.
 */
uint32_t cfgpack_crc32c(const uint8_t *data, size_t len);

#endif
