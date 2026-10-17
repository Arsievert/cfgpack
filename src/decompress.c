/**
 * @file decompress.c
 * @brief Optional decompression support for cfgpack.
 *
 * Implements LZ4 and heatshrink decompression wrappers.
 * Enabled via CFGPACK_LZ4 and CFGPACK_HEATSHRINK compile flags.
 */

#include "cfgpack/decompress.h"

#include <limits.h>

#ifdef CFGPACK_LZ4
  #include "lz4.h"

cfgpack_err_t cfgpack_pagein_lz4(cfgpack_ctx_t *ctx,
                                 const uint8_t *data,
                                 size_t len,
                                 size_t decompressed_size,
                                 uint8_t *scratch,
                                 size_t scratch_cap) {
    int result;

    if (!ctx || !data || !scratch) {
        return (CFGPACK_ERR_DECODE);
    }
    if (decompressed_size > scratch_cap) {
        return (CFGPACK_ERR_BOUNDS);
    }
    /* LZ4_decompress_safe takes int sizes; refuse anything that would wrap */
    if (len > (size_t)INT_MAX || decompressed_size > (size_t)INT_MAX) {
        return (CFGPACK_ERR_BOUNDS);
    }

    /* LZ4_decompress_safe requires knowing the exact decompressed size */
    result = LZ4_decompress_safe((const char *)data, (char *)scratch, (int)len,
                                 (int)decompressed_size);

    if (result < 0) {
        return (CFGPACK_ERR_DECODE);
    }
    if ((size_t)result != decompressed_size) {
        return (CFGPACK_ERR_DECODE);
    }

    return (cfgpack_pagein_buf(ctx, scratch, (size_t)result));
}

#endif /* CFGPACK_LZ4 */

#ifdef CFGPACK_HEATSHRINK
  #include "heatshrink_decoder.h"

/* Static decoder instance (no dynamic allocation) */
static heatshrink_decoder hs_decoder;

/**
 * @brief Drain pending decoder output into the scratch buffer.
 *
 * heatshrink_decoder_poll() returns HSDR_POLL_MORE whenever it fills the
 * output buffer it was given -- including a zero-length buffer -- so polling
 * again once scratch is full would spin forever.  A full scratch buffer is
 * therefore detected here: while compressed input is still pending it is an
 * overflow, and once all input has been sunk only the decoder's finish()
 * state can tell an exact fit (DONE) from a truncated result (MORE).
 *
 * @param scratch      Output buffer.
 * @param scratch_cap  Capacity of @p scratch.
 * @param total_output In/out: bytes written so far.
 * @param input_done   Non-zero once every compressed byte has been sunk.
 * @return CFGPACK_OK when the decoder has no more output it can produce
 *         right now; CFGPACK_ERR_BOUNDS if scratch is too small;
 *         CFGPACK_ERR_DECODE on decoder error.
 */
static cfgpack_err_t hs_drain(uint8_t *scratch,
                              size_t scratch_cap,
                              size_t *total_output,
                              int input_done) {
    HSD_poll_res poll_res;
    size_t produced;

    do {
        if (*total_output == scratch_cap) {
            if (input_done &&
                heatshrink_decoder_finish(&hs_decoder) == HSDR_FINISH_DONE) {
                return (CFGPACK_OK);
            }
            return (CFGPACK_ERR_BOUNDS);
        }
        poll_res = heatshrink_decoder_poll(&hs_decoder, scratch + *total_output,
                                           scratch_cap - *total_output,
                                           &produced);
        if (poll_res < 0) {
            return (CFGPACK_ERR_DECODE);
        }
        *total_output += produced;
    } while (poll_res == HSDR_POLL_MORE);

    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_pagein_heatshrink(cfgpack_ctx_t *ctx,
                                        const uint8_t *data,
                                        size_t len,
                                        uint8_t *scratch,
                                        size_t scratch_cap) {
    size_t input_consumed = 0;
    HSD_finish_res finish_res;
    size_t total_output = 0;
    HSD_sink_res sink_res;
    cfgpack_err_t rc;
    size_t sunk;

    if (!ctx || !data || !scratch) {
        return (CFGPACK_ERR_DECODE);
    }

    heatshrink_decoder_reset(&hs_decoder);

    /* Feed compressed data and drain output after each chunk */
    while (input_consumed < len) {
        sink_res = heatshrink_decoder_sink(&hs_decoder,
                                           (uint8_t *)(data + input_consumed),
                                           len - input_consumed, &sunk);
        if (sink_res < 0 || sunk == 0) {
            return (CFGPACK_ERR_DECODE);
        }
        input_consumed += sunk;

        rc = hs_drain(scratch, scratch_cap, &total_output,
                      input_consumed == len);
        if (rc != CFGPACK_OK) {
            return (rc);
        }
    }

    /* Notify decoder that input is finished and drain what remains */
    finish_res = heatshrink_decoder_finish(&hs_decoder);
    while (finish_res == HSDR_FINISH_MORE) {
        rc = hs_drain(scratch, scratch_cap, &total_output, 1);
        if (rc != CFGPACK_OK) {
            return (rc);
        }
        finish_res = heatshrink_decoder_finish(&hs_decoder);
    }
    if (finish_res < 0) {
        return (CFGPACK_ERR_DECODE);
    }

    return (cfgpack_pagein_buf(ctx, scratch, total_output));
}

#endif /* CFGPACK_HEATSHRINK */
