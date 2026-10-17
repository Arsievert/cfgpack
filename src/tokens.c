#include "tokens.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @copydoc tokens_create */
int tokens_create(tokens_t *tokens, uint16_t n, char **provided) {
    tokens->index = NULL;
    tokens->max = 0;
    tokens->used = 0;

    if (!provided || n == 0) {
        return (-1);
    }
    tokens->index = provided;
    tokens->max = n;
    return (0);
}

/**
 * @brief Test whether a character is in the delimiter set.
 *
 * @param s Character to test.
 * @param d NUL-terminated delimiter string.
 * @return 1 if delimiter, 0 otherwise.
 */
static int is_delimiter(char s, const char *d) {
    for (; *d; d++) {
        if (*d == s) {
            return (1);
        }
    }
    return (0);
}

/** @copydoc tokens_find */
int tokens_find(tokens_t *tokens,
                char *input,
                const char *delimiters,
                uint16_t n_tokens,
                size_t *stop_offset) {
    bool new_token;
    size_t len;

    if ((!input) || (!delimiters) || (!n_tokens) || (!tokens->index)) {
        return (-1);
    }

    if (n_tokens > tokens->max) {
        return (-1);
    }

    tokens->used = 0;
    new_token = true;

    /* Use the length of the string before mutilation. */
    len = strlen(input);

    for (size_t i = 0; i < len; i++) {
        if (is_delimiter(input[i], delimiters)) {
            input[i] = 0;
            new_token = true;
            continue;
        }

        if (new_token) {
            if (tokens->used >= n_tokens) {
                if (stop_offset) {
                    *stop_offset = i;
                }
                return (0);
            }
            tokens->index[tokens->used++] = (char *)&input[i];
            new_token = false;
        }
    }

    if (stop_offset) {
        *stop_offset = len;
    }

    return (0);
}

/** @copydoc tokens_destroy */
int tokens_destroy(tokens_t *tokens) {
    tokens->index = NULL;
    tokens->used = 0;
    tokens->max = 0;

    return (0);
}
