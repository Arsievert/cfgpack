#ifndef CFGPACK_TOKENS_H
#define CFGPACK_TOKENS_H

/**
 * @file tokens.h
 * @brief In-place string tokenizer utility for cfgpack schema parsing.
 *
 * Splits a mutable input string into tokens by inserting NUL terminators
 * at delimiter positions.  The index buffer is always caller-provided; the
 * tokenizer never allocates.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * @brief In-place string tokenizer state.
 */
typedef struct tokens_struct {
    char **index;  /**< @brief Array of pointers to each token start. */
    uint16_t max;  /**< @brief Maximum number of token slots available. */
    uint16_t used; /**< @brief Current number of tokens found. */
} tokens_t;

/**
 * @brief Initialize a token container over a caller-provided index buffer.
 *
 * @param tokens   Token container to initialize.
 * @param n        Number of slots in @p provided.
 * @param provided Caller-provided index buffer of at least @p n pointers.
 * @return 0 on success, -1 if @p provided is NULL or @p n is 0.
 */
int tokens_create(tokens_t *tokens, uint16_t n, char **provided);

/**
 * @brief Tokenize an input string in-place using delimiters.
 *
 * Scans @p input, replacing delimiter characters with NUL terminators and
 * recording the start of each token in @p tokens.  Stops after @p n_tokens
 * tokens; the rest of the input is left untouched and @p stop_offset (if
 * non-NULL) receives the byte offset of the first unconsumed token, or the
 * input length if the whole string was consumed.
 *
 * @param tokens      Token container (initialized via tokens_create).
 * @param input       Mutable input string (modified in place).
 * @param delimiters  Delimiter characters (each char is a separator).
 * @param n_tokens    Maximum number of tokens to extract (<= tokens->max).
 * @param stop_offset Optional: where scanning stopped.
 * @return 0 on success, -1 on invalid arguments.
 */
int tokens_find(tokens_t *tokens,
                char *input,
                const char *delimiters,
                uint16_t n_tokens,
                size_t *stop_offset);

/**
 * @brief Reset a token container to an empty state.
 *
 * Nothing is freed; the index buffer belongs to the caller.
 *
 * @param tokens Token container to reset.
 * @return 0 always.
 */
int tokens_destroy(tokens_t *tokens);

#endif /* CFGPACK_TOKENS_H */
