#include "test.h"

test_result_t test_case_result(const char *name, test_result_t r) {
    if (r == TEST_OK) {
        printf("PASS %s\n", name);
    } else {
        printf("FAIL %s\n", name);
    }
    return (r);
}
