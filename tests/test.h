#ifndef CFGPACK_TEST_H
#define CFGPACK_TEST_H

#include <stdio.h>

/* ANSI color codes */
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"

typedef enum {
    TEST_OK = 0,
    TEST_FAIL = 1
} test_result_t;

#define TEST_CASE(name) \
    test_result_t name(void)

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, COLOR_RED "FAIL" COLOR_RESET " %s:%d: %s\n", __FILE__, __LINE__, #expr); \
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
