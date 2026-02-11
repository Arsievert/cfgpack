/* tokens.h
 *
 * Author: Austin Sievert (arsievert1@gmail.com)
 * URL:    https://github.com/arsievert/cfgpack
 *
 * License: MIT
 */

#ifndef _TOKENS_H_
#define _TOKENS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct tokens_struct {
    /* max
     * number of tokens that can be used. */
    uint16_t max;
    /* used
     * current number of tokens in use. */
    uint16_t used;
    /* index
     * pointer used to iterate over individual tokens. */
    char **index;
    /* owns
     * whether the index buffer was allocated internally. */
    bool owns;
} tokens_t;

int tokens_create(tokens_t *, uint16_t, char **);
int tokens_find(tokens_t *, char *, const char *, uint16_t, size_t *);
int tokens_destroy(tokens_t *);

#endif /* _TOKENS_H_ */
