#ifndef _TOKENS_H_
#define _TOKENS_H_

/**
 * @file tokens.h
 * @brief In-place string tokenizer utility for cfgpack schema parsing.
 *
 * Splits a mutable input string into tokens by inserting NUL terminators
 * at delimiter positions. Supports both caller-provided and internally
 * allocated index buffers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief In-place string tokenizer state.
 */
typedef struct tokens_struct {
    uint16_t max;  /**< @brief Maximum number of token slots available. */
    uint16_t used; /**< @brief Current number of tokens found. */
    char **index;  /**< @brief Array of pointers to each token start. */
    bool owns;     /**< @brief Whether @c index was allocated internally. */
} tokens_t;

/**
 * @brief Initialize a token container.
 *
 * If @p provided is non-NULL it is used as the index buffer; otherwise
 * an internal buffer of @p n slots is allocated via @c malloc.
 *
 * @param tokens   Token container to initialize.
 * @param n        Maximum number of token slots.
 * @param provided Caller-provided index buffer, or @c NULL to allocate.
 * @return 0 on success, -1 on allocation failure.
 */
int tokens_create(tokens_t *tokens, uint16_t n, char **provided);

/**
 * @brief Tokenize an input string in-place using delimiters.
 *
 * Scans @p input, replacing delimiter characters with NUL terminators and
 * recording the start of each token in @p tokens. Stops when @p n_tokens
 * have been found or the end of the string is reached.
 *
 * @param tokens      Token container (must be initialized via tokens_create).
 * @param input       Mutable input string (modified in place).
 * @param delimeters  Delimiter characters (each char is a separator).
 * @param n_tokens    Maximum number of tokens to extract.
 * @param stop_offset If non-NULL, set to the byte offset where parsing stopped.
 * @return 0 on success, -1 on invalid arguments or capacity exceeded.
 */
int tokens_find(tokens_t *tokens,
                char *input,
                const char *delimeters,
                uint16_t n_tokens,
                size_t *stop_offset);

/**
 * @brief Destroy a token container and free any internally allocated memory.
 *
 * If the index buffer was allocated by tokens_create, it is freed.
 * The container is reset to a safe zero state.
 *
 * @param tokens Token container to destroy.
 * @return 0 always.
 */
int tokens_destroy(tokens_t *tokens);

#endif /* _TOKENS_H_ */
