/**
 * @file decompress.c
 * @brief Optional decompression support for cfgpack.
 *
 * Implements LZ4 and heatshrink decompression wrappers.
 * Enabled via CFGPACK_LZ4 and CFGPACK_HEATSHRINK compile flags.
 */

#include "cfgpack/decompress.h"

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
        return CFGPACK_ERR_DECODE;
    }
    if (decompressed_size > scratch_cap) {
        return CFGPACK_ERR_BOUNDS;
    }

    /* LZ4_decompress_safe requires knowing the exact decompressed size */
    result = LZ4_decompress_safe((const char *)data, (char *)scratch,
                                 (int)len, (int)decompressed_size);

    if (result < 0) {
        return CFGPACK_ERR_DECODE;
    }
    if ((size_t)result != decompressed_size) {
        return CFGPACK_ERR_DECODE;
    }

    return cfgpack_pagein_buf(ctx, scratch, (size_t)result);
}

#endif /* CFGPACK_LZ4 */

#ifdef CFGPACK_HEATSHRINK
  #include "heatshrink_decoder.h"

/* Static decoder instance (no dynamic allocation) */
static heatshrink_decoder hs_decoder;

cfgpack_err_t cfgpack_pagein_heatshrink(cfgpack_ctx_t *ctx,
                                        const uint8_t *data,
                                        size_t len,
                                        uint8_t *scratch,
                                        size_t scratch_cap) {
    size_t input_consumed = 0;
    size_t output_produced = 0;
    size_t total_output = 0;
    HSD_sink_res sink_res;
    HSD_poll_res poll_res;
    HSD_finish_res finish_res;

    if (!ctx || !data || !scratch) {
        return CFGPACK_ERR_DECODE;
    }

    heatshrink_decoder_reset(&hs_decoder);

    /* Feed compressed data and poll for output */
    while (input_consumed < len) {
        /* Sink input data */
        sink_res = heatshrink_decoder_sink(&hs_decoder,
                                           (uint8_t *)(data + input_consumed),
                                           len - input_consumed,
                                           &output_produced);
        if (sink_res < 0) {
            return CFGPACK_ERR_DECODE;
        }
        input_consumed += output_produced;

        /* Poll for decompressed output */
        do {
            poll_res = heatshrink_decoder_poll(&hs_decoder,
                                               scratch + total_output,
                                               scratch_cap -
                                                   total_output,
                                               &output_produced);
            if (poll_res < 0) {
                return CFGPACK_ERR_DECODE;
            }
            total_output += output_produced;

            if (total_output > scratch_cap) {
                return CFGPACK_ERR_BOUNDS;
            }
        } while (poll_res == HSDR_POLL_MORE);
    }

    /* Notify decoder that input is finished */
    finish_res = heatshrink_decoder_finish(&hs_decoder);
    if (finish_res < 0) {
        return CFGPACK_ERR_DECODE;
    }

    /* Continue polling until done */
    while (finish_res == HSDR_FINISH_MORE) {
        poll_res = heatshrink_decoder_poll(&hs_decoder,
                                           scratch + total_output,
                                           scratch_cap - total_output,
                                           &output_produced);
        if (poll_res < 0) {
            return CFGPACK_ERR_DECODE;
        }
        total_output += output_produced;

        if (total_output > scratch_cap) {
            return CFGPACK_ERR_BOUNDS;
        }

        finish_res = heatshrink_decoder_finish(&hs_decoder);
        if (finish_res < 0) {
            return CFGPACK_ERR_DECODE;
        }
    }

    return cfgpack_pagein_buf(ctx, scratch, total_output);
}

#endif /* CFGPACK_HEATSHRINK */
