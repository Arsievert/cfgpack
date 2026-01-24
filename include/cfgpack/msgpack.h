#ifndef CFGPACK_MSGPACK_H
#define CFGPACK_MSGPACK_H

/**
 * @file msgpack.h
 * @brief MessagePack encoding and decoding primitives.
 *
 * Provides low-level MessagePack serialization helpers used internally
 * by cfgpack. These can also be used directly for custom encoding needs.
 */

#include <stddef.h>
#include <stdint.h>

#include "error.h"

/**
 * @brief Fixed-capacity buffer used for MessagePack encoding.
 */
typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} cfgpack_buf_t;

/**
 * @brief Initialize a fixed-capacity buffer for MessagePack encoding.
 *
 * @param buf     Buffer to initialize.
 * @param storage Caller-provided backing storage.
 * @param cap     Capacity of @p storage in bytes.
 */
void cfgpack_buf_init(cfgpack_buf_t *buf, uint8_t *storage, size_t cap);

/**
 * @brief Append bytes to a MessagePack buffer.
 *
 * @param buf Buffer to append to.
 * @param src Source bytes to copy.
 * @param len Number of bytes to append.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if capacity exceeded.
 */
cfgpack_err_t cfgpack_buf_append(cfgpack_buf_t *buf, const void *src, size_t len);

/** Encoding helpers (MessagePack subset). */

/**
 * @brief Encode an unsigned 64-bit integer to MessagePack format.
 * @param buf Buffer to append encoded data to.
 * @param v   Value to encode.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_uint64(cfgpack_buf_t *buf, uint64_t v);

/**
 * @brief Encode a signed 64-bit integer to MessagePack format.
 * @param buf Buffer to append encoded data to.
 * @param v   Value to encode.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_int64(cfgpack_buf_t *buf, int64_t v);

/**
 * @brief Encode a 32-bit float to MessagePack format.
 * @param buf Buffer to append encoded data to.
 * @param v   Value to encode.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_f32(cfgpack_buf_t *buf, float v);

/**
 * @brief Encode a 64-bit double to MessagePack format.
 * @param buf Buffer to append encoded data to.
 * @param v   Value to encode.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_f64(cfgpack_buf_t *buf, double v);

/**
 * @brief Encode a string to MessagePack format.
 * @param buf Buffer to append encoded data to.
 * @param s   String to encode (not null-terminated required).
 * @param len Length of string in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_str(cfgpack_buf_t *buf, const char *s, size_t len);

/**
 * @brief Encode a MessagePack map header with the given entry count.
 * @param buf   Buffer to append encoded data to.
 * @param count Number of key-value pairs in the map.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_map_header(cfgpack_buf_t *buf, uint32_t count);

/**
 * @brief Encode an unsigned integer as a MessagePack map key.
 * @param buf Buffer to append encoded data to.
 * @param v   Key value to encode.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_uint_key(cfgpack_buf_t *buf, uint64_t v);

/**
 * @brief Encode a string as a MessagePack map key.
 * @param buf Buffer to append encoded data to.
 * @param s   Key string to encode.
 * @param len Length of key string in bytes.
 * @return CFGPACK_OK on success; CFGPACK_ERR_ENCODE if buffer capacity exceeded.
 */
cfgpack_err_t cfgpack_msgpack_encode_str_key(cfgpack_buf_t *buf, const char *s, size_t len);

/**
 * @brief Reader over a MessagePack buffer.
 */
typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
} cfgpack_reader_t;

/**
 * @brief Initialize a MessagePack reader.
 *
 * @param r    Reader to initialize.
 * @param data Buffer to read from.
 * @param len  Length of @p data in bytes.
 */
void cfgpack_reader_init(cfgpack_reader_t *r, const uint8_t *data, size_t len);

/** Decoding helpers (MessagePack subset). */

/**
 * @brief Decode an unsigned 64-bit integer from MessagePack format.
 * @param r   Reader positioned at the encoded value.
 * @param out Output parameter for decoded value.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on format error or EOF.
 */
cfgpack_err_t cfgpack_msgpack_decode_uint64(cfgpack_reader_t *r, uint64_t *out);

/**
 * @brief Decode a signed 64-bit integer from MessagePack format.
 * @param r   Reader positioned at the encoded value.
 * @param out Output parameter for decoded value.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on format error or EOF.
 */
cfgpack_err_t cfgpack_msgpack_decode_int64(cfgpack_reader_t *r, int64_t *out);

/**
 * @brief Decode a 32-bit float from MessagePack format.
 * @param r   Reader positioned at the encoded value.
 * @param out Output parameter for decoded value.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on format error or EOF.
 */
cfgpack_err_t cfgpack_msgpack_decode_f32(cfgpack_reader_t *r, float *out);

/**
 * @brief Decode a 64-bit double from MessagePack format.
 * @param r   Reader positioned at the encoded value.
 * @param out Output parameter for decoded value.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on format error or EOF.
 */
cfgpack_err_t cfgpack_msgpack_decode_f64(cfgpack_reader_t *r, double *out);

/**
 * @brief Decode a string from MessagePack format.
 * @param r   Reader positioned at the encoded value.
 * @param ptr Output parameter for pointer into reader's buffer (not null-terminated).
 * @param len Output parameter for string length.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on format error or EOF.
 */
cfgpack_err_t cfgpack_msgpack_decode_str(cfgpack_reader_t *r, const uint8_t **ptr, uint32_t *len);

/**
 * @brief Decode a MessagePack map header.
 * @param r     Reader positioned at the map header.
 * @param count Output parameter for number of key-value pairs.
 * @return CFGPACK_OK on success; CFGPACK_ERR_DECODE on format error or EOF.
 */
cfgpack_err_t cfgpack_msgpack_decode_map_header(cfgpack_reader_t *r, uint32_t *count);

#endif /* CFGPACK_MSGPACK_H */
