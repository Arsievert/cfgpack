#ifndef _WBUF_H_
#define _WBUF_H_

/**
 * @file wbuf.h
 * @brief Write buffer utility for cfgpack schema serialization.
 *
 * Provides a simple write buffer that tracks total bytes needed even
 * when the destination buffer overflows, enabling measure-then-write
 * patterns without heap allocation.
 */

#include <stddef.h>

/**
 * @brief Write buffer that tracks total bytes needed, even when overflowing.
 *
 * Writes to a caller-provided buffer.  When the buffer is full further data is
 * silently dropped, but @c len keeps incrementing so the caller can discover
 * the exact size required by a second (measure) pass.
 */
typedef struct {
    char *buf;  /**< @brief Caller-provided output buffer. */
    size_t cap; /**< @brief Capacity of @c buf in bytes. */
    size_t len; /**< @brief Total bytes written (may exceed @c cap). */
} wbuf_t;

/**
 * @brief Initialize a write buffer with a caller-provided backing store.
 *
 * @param w   Write buffer to initialize.
 * @param buf Destination buffer (may be @c NULL if @p cap is 0).
 * @param cap Capacity of @p buf in bytes.
 */
void wbuf_init(wbuf_t *w, char *buf, size_t cap);

/**
 * @brief Append @p n bytes from @p s to the write buffer.
 *
 * If the buffer is full, the data is silently dropped but @c len still
 * advances so the caller can determine the required size.  Partial writes
 * are performed when some but not all bytes fit.
 *
 * @param w Write buffer.
 * @param s Source data to append.
 * @param n Number of bytes to append.
 */
void wbuf_append(wbuf_t *w, const char *s, size_t n);

/**
 * @brief Append a NUL-terminated string (without the NUL).
 *
 * @param w Write buffer.
 * @param s NUL-terminated string to append.
 */
void wbuf_puts(wbuf_t *w, const char *s);

/**
 * @brief Append a single character.
 *
 * @param w Write buffer.
 * @param c Character to append.
 */
void wbuf_putc(wbuf_t *w, char c);

/**
 * @brief Format an unsigned integer as decimal text and append it.
 *
 * Does not use @c snprintf; suitable for embedded targets without stdio.
 *
 * @param w   Write buffer.
 * @param val Value to format.
 */
void wbuf_put_uint(wbuf_t *w, unsigned long long val);

/**
 * @brief Format a signed integer as decimal text and append it.
 *
 * Negative values are prefixed with @c '-'.
 *
 * @param w   Write buffer.
 * @param val Value to format.
 */
void wbuf_put_int(wbuf_t *w, long long val);

/**
 * @brief Format a double-precision float as text and append it.
 *
 * On hosted builds (@c CFGPACK_HOSTED) this uses @c snprintf with
 * <tt>"%.17g"</tt> formatting.  On embedded builds a minimal integer +
 * fractional digit formatter is used (up to 9 fractional digits, trailing
 * zeros trimmed).
 *
 * @param w   Write buffer.
 * @param val Value to format.
 */
void wbuf_put_double(wbuf_t *w, double val);

/**
 * @brief Format a single-precision float as text and append it.
 *
 * On hosted builds (@c CFGPACK_HOSTED) this uses @c snprintf with
 * <tt>"%.9g"</tt> formatting.  On embedded builds this delegates to
 * @ref wbuf_put_double.
 *
 * @param w   Write buffer.
 * @param val Value to format.
 */
void wbuf_put_float(wbuf_t *w, float val);

#endif /* _WBUF_H_ */
