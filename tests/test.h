#ifndef CFGPACK_TEST_H
#define CFGPACK_TEST_H

#include <stdio.h>

typedef enum {
    TEST_OK = 0,
    TEST_FAIL = 1
} test_result_t;

#define TEST_CASE(name) \
    test_result_t name(void)

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return (TEST_FAIL); \
    } \
} while (0)

static inline test_result_t test_pass(const char *name) {
    (void)name;
    return (TEST_OK);
}

static inline test_result_t test_fail(const char *name) {
    (void)name;
    return (TEST_FAIL);
}

test_result_t test_case_result(const char *name, test_result_t r);

#endif /* CFGPACK_TEST_H */
