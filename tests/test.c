#include "test.h"

/* ANSI color codes */
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"

test_result_t test_case_result(const char *name, test_result_t r) {
    if (r == TEST_OK) {
        printf(COLOR_GREEN "PASS" COLOR_RESET " %s\n", name);
    } else {
        printf(COLOR_RED "FAIL" COLOR_RESET " %s\n", name);
    }
    return (r);
}
