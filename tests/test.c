#include "test.h"

test_result_t test_case_result(const char *name, test_result_t r) {
    if (r == TEST_OK) {
        printf(COLOR_GREEN "PASS" COLOR_RESET " %s\n", name);
    } else {
        printf(COLOR_RED "FAIL" COLOR_RESET " %s\n", name);
    }
    return (r);
}
