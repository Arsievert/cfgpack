#include "cfgpack/config.h"

#include <stdint.h>
#include <string.h>

#ifdef CFGPACK_HOSTED
  #include <stdio.h>
#endif

#include "wbuf.h"

/** @copydoc wbuf_init */
void wbuf_init(wbuf_t *w, char *buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
}

/** @copydoc wbuf_append */
void wbuf_append(wbuf_t *w, const char *s, size_t n) {
    if (w->len + n <= w->cap) {
        memcpy(w->buf + w->len, s, n);
    } else if (w->len < w->cap) {
        /* Partial write */
        size_t avail = w->cap - w->len;
        memcpy(w->buf + w->len, s, avail);
    }
    w->len += n; /* Always increment to track needed size */
}

/** @copydoc wbuf_puts */
void wbuf_puts(wbuf_t *w, const char *s) {
    wbuf_append(w, s, strlen(s));
}

/** @copydoc wbuf_putc */
void wbuf_putc(wbuf_t *w, char c) {
    wbuf_append(w, &c, 1);
}

/** @copydoc wbuf_put_uint */
void wbuf_put_uint(wbuf_t *w, unsigned long long val) {
    char tmp[24];
    int i = 0;
    if (val == 0) {
        wbuf_putc(w, '0');
        return;
    }
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        wbuf_putc(w, tmp[--i]);
    }
}

/** @copydoc wbuf_put_int */
void wbuf_put_int(wbuf_t *w, long long val) {
    if (val < 0) {
        wbuf_putc(w, '-');
        val = -val;
    }
    wbuf_put_uint(w, (unsigned long long)val);
}

/** @copydoc wbuf_put_double */
void wbuf_put_double(wbuf_t *w, double val) {
#ifdef CFGPACK_HOSTED
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%.17g", val);
    if (len > 0 && (size_t)len < sizeof(tmp)) {
        wbuf_append(w, tmp, (size_t)len);
    }
#else
    /* Minimal formatter: sign, integer part, decimal point, up to 9 fractional digits */
    uint64_t ipart;
    double frac;
    uint64_t frac_int;
    char digits[9];
    int ndigits;

    if (val < 0) {
        wbuf_putc(w, '-');
        val = -val;
    }
    ipart = (uint64_t)val;
    wbuf_put_uint(w, ipart);

    frac = val - (double)ipart;
    if (frac > 1e-10) { /* Skip if essentially zero */
        wbuf_putc(w, '.');
        /* Multiply by 10^9 and round to get all digits at once - avoids accumulated error */
        frac_int = (uint64_t)(frac * 1000000000.0 + 0.5);
        /* Extract up to 9 digits */
        ndigits = 9;
        for (int i = 8; i >= 0; i--) {
            digits[i] = '0' + (frac_int % 10);
            frac_int /= 10;
        }
        /* Trim trailing zeros */
        while (ndigits > 0 && digits[ndigits - 1] == '0') {
            ndigits--;
        }
        for (int i = 0; i < ndigits; i++) {
            wbuf_putc(w, digits[i]);
        }
    }
#endif
}

/** @copydoc wbuf_put_float */
void wbuf_put_float(wbuf_t *w, float val) {
#ifdef CFGPACK_HOSTED
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%.9g", (double)val);
    if (len > 0 && (size_t)len < sizeof(tmp)) {
        wbuf_append(w, tmp, (size_t)len);
    }
#else
    wbuf_put_double(w, (double)val);
#endif
}

/** @copydoc wbuf_try_append */
cfgpack_err_t wbuf_try_append(wbuf_t *w, const void *src, size_t n) {
    if (w->len + n > w->cap) {
        w->len += n;
        return (CFGPACK_ERR_ENCODE);
    }
    memcpy(w->buf + w->len, src, n);
    w->len += n;
    return (CFGPACK_OK);
}
