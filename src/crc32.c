/**
 * @file crc32.c
 * @brief CRC-32C (Castagnoli) implementation for cfgpack blob integrity.
 *
 * Nibble-at-a-time algorithm using reflected polynomial 0x82F63B78.
 * Lookup table is 16 entries (64 bytes) — good speed/size tradeoff
 * for embedded targets.
 */

#include "crc32.h"

/* CRC-32C (Castagnoli) nibble table — reflected polynomial 0x82F63B78 */
static const uint32_t crc_table[16] = {0x00000000, 0x105EC76F, 0x20BD8EDE,
                                       0x30E349B1, 0x417B1DBC, 0x5125DAD3,
                                       0x61C69362, 0x7198540D, 0x82F63B78,
                                       0x92A8FC17, 0xA24BB5A6, 0xB21572C9,
                                       0xC38D26C4, 0xD3D3E1AB, 0xE330A81A,
                                       0xF36E6F75};

uint32_t cfgpack_crc32c(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = crc_table[(crc ^ data[i]) & 0x0F] ^ (crc >> 4);
        crc = crc_table[(crc ^ (data[i] >> 4)) & 0x0F] ^ (crc >> 4);
    }
    return (~crc);
}
