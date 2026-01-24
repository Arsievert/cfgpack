/* tokens.c
 *
 * Author: Austin Sievert (arsievert1@gmail.com)
 * URL:    https://github.com/arsievert/cfgpack
 *
 * License: MIT
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "tokens.h"

int
tokens_create(tokens_t *tokens, uint16_t n, char **provided)
{
    tokens->max = 0;
    tokens->used = 0;
    tokens->owns = false;

    if (provided) {
        tokens->index = provided;
    } else {
        tokens->index = (char **)malloc(n * sizeof(char *));
        tokens->owns = (tokens->index != NULL);
    }

    if (tokens->index) {
        tokens->max = n;
        return 0;
    }

    return -1;
}

/**
 * @brief Test whether a character is in the delimiter set.
 *
 * @param s Character to test.
 * @param d NUL-terminated delimiter string.
 * @return 1 if delimiter, 0 otherwise.
 */
static int
_is_delimeter(char s, const char *d)
{
    int len;

    if (!d) {
        return (0);
    }

    len = strlen(d);

    for (int i = 0; i < len; i++) {
        if (d[i] == s) {
            return (1);
        }
    }

    return (0);
}

/**
 * @brief Tokenize an input string in-place using delimiters.
 *
 * @param tokens       Token container to fill.
 * @param input        Mutable input string to split (NUL terminators inserted).
 * @param delimeters   Delimiter characters (string of separators).
 * @param n_tokens     Maximum tokens to extract.
 * @param stop_offset  Optional offset where parsing stopped.
 * @return 0 on success; -1 on invalid args or capacity issues.
 */
int
tokens_find(tokens_t *tokens, char *input, const char *delimeters, uint16_t n_tokens, size_t *stop_offset)
{
    size_t len;
    bool new_token;

    if ((!input) ||
        (!delimeters) ||
        (!n_tokens) ||
        (!tokens->index)) {
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
        if (_is_delimeter(input[i], delimeters)) {
            input[i] = 0;
            new_token = true;
            continue;
        }

        if (new_token) {
            if (tokens->used >= n_tokens || tokens->used >= tokens->max) {
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

int
tokens_destroy(tokens_t *tokens)
{
    if (tokens->index && tokens->owns) {
        free(tokens->index);
    }

    tokens->index = NULL;
    tokens->used = 0;
    tokens->max = 0;
    tokens->owns = false;

    return 0;
}
