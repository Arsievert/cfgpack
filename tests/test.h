#ifndef CFGPACK_TEST_H
#define CFGPACK_TEST_H

#include <stdio.h>
#include <inttypes.h>

/* ANSI color codes */
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_DIM "\033[2m"
#define COLOR_RESET "\033[0m"

typedef enum { TEST_OK = 0, TEST_FAIL = 1 } test_result_t;

#define TEST_CASE(name) test_result_t name(void)

/* Verbose logging macro - prints indented, dimmed text */
#define LOG(fmt, ...)                                                          \
    printf(COLOR_DIM "    " fmt COLOR_RESET "\n", ##__VA_ARGS__)

/* Log a section header within a test */
#define LOG_SECTION(title)                                                     \
    printf(COLOR_CYAN "    --- " title " ---" COLOR_RESET "\n")

/* Log a hex dump of a buffer */
#define LOG_HEX(label, buf, len)                                               \
    do {                                                                       \
        printf(COLOR_DIM "    %s [%zu bytes]: ", label, (size_t)(len));        \
        for (size_t _i = 0; _i < (size_t)(len) && _i < 32; _i++) {             \
            printf("%02x ", ((const uint8_t *)(buf))[_i]);                     \
        }                                                                      \
        if ((size_t)(len) > 32)                                                \
            printf("...");                                                     \
        printf(COLOR_RESET "\n");                                              \
    } while (0)

/* Log a value with type information (for cfgpack_value_t) */
#define LOG_VALUE(label, val)                                                  \
    do {                                                                       \
        switch ((val).type) {                                                  \
        case CFGPACK_TYPE_U8:                                                  \
            LOG("%s: u8 = %" PRIu64, label, (val).v.u64);                      \
            break;                                                             \
        case CFGPACK_TYPE_U16:                                                 \
            LOG("%s: u16 = %" PRIu64, label, (val).v.u64);                     \
            break;                                                             \
        case CFGPACK_TYPE_U32:                                                 \
            LOG("%s: u32 = %" PRIu64, label, (val).v.u64);                     \
            break;                                                             \
        case CFGPACK_TYPE_U64:                                                 \
            LOG("%s: u64 = %" PRIu64, label, (val).v.u64);                     \
            break;                                                             \
        case CFGPACK_TYPE_I8:                                                  \
            LOG("%s: i8 = %" PRId64, label, (val).v.i64);                      \
            break;                                                             \
        case CFGPACK_TYPE_I16:                                                 \
            LOG("%s: i16 = %" PRId64, label, (val).v.i64);                     \
            break;                                                             \
        case CFGPACK_TYPE_I32:                                                 \
            LOG("%s: i32 = %" PRId64, label, (val).v.i64);                     \
            break;                                                             \
        case CFGPACK_TYPE_I64:                                                 \
            LOG("%s: i64 = %" PRId64, label, (val).v.i64);                     \
            break;                                                             \
        case CFGPACK_TYPE_F32:                                                 \
            LOG("%s: f32 = %f", label, (double)(val).v.f32);                   \
            break;                                                             \
        case CFGPACK_TYPE_F64: LOG("%s: f64 = %f", label, (val).v.f64); break; \
        case CFGPACK_TYPE_STR:                                                 \
            LOG("%s: str[%u] @offset=%u", label, (val).v.str.len,              \
                (val).v.str.offset);                                           \
            break;                                                             \
        case CFGPACK_TYPE_FSTR:                                                \
            LOG("%s: fstr[%u] @offset=%u", label, (val).v.fstr.len,            \
                (val).v.fstr.offset);                                          \
            break;                                                             \
        default: LOG("%s: unknown type %d", label, (val).type); break;         \
        }                                                                      \
    } while (0)

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, COLOR_RED "    FAIL" COLOR_RESET " %s:%d: %s\n",   \
                    __FILE__, __LINE__, #expr);                                \
            return (TEST_FAIL);                                                \
        }                                                                      \
    } while (0)

/* CHECK with logging - logs success on pass */
#define CHECK_LOG(expr, fmt, ...)                                              \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, COLOR_RED "    FAIL" COLOR_RESET " %s:%d: %s\n",   \
                    __FILE__, __LINE__, #expr);                                \
            return (TEST_FAIL);                                                \
        }                                                                      \
        LOG(fmt, ##__VA_ARGS__);                                               \
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
