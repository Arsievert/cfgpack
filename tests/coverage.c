/* Coverage improvement tests for api.h typed wrappers, core.c error paths,
 * and io_file.c untested branches. */

#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"

#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

static void make_schema(cfgpack_schema_t *schema,
                        cfgpack_entry_t *entries,
                        size_t n) {
    snprintf(schema->map_name, sizeof(schema->map_name), "test");
    schema->version = 1;
    schema->entry_count = n;
    schema->entries = entries;
    for (size_t i = 0; i < n; ++i) {
        entries[i].index = (uint16_t)(i + 1);
        snprintf(entries[i].name, sizeof(entries[i].name), "e%zu", i);
        entries[i].type = CFGPACK_TYPE_U8;
        entries[i].has_default = 0;
    }
}

/* Build a 12-entry schema covering all types: u8 u16 u32 u64 i8 i16 i32 i64
 * f32 f64 str fstr, with entry names t0..t11. */
static cfgpack_err_t make_all_types_ctx(cfgpack_ctx_t *ctx,
                                        cfgpack_schema_t *schema,
                                        cfgpack_entry_t *entries,
                                        cfgpack_value_t *values,
                                        char *str_pool,
                                        size_t str_pool_cap,
                                        uint16_t *str_offsets,
                                        size_t str_offsets_count) {
    static const cfgpack_type_t types[] = {
        CFGPACK_TYPE_U8,  CFGPACK_TYPE_U16, CFGPACK_TYPE_U32, CFGPACK_TYPE_U64,
        CFGPACK_TYPE_I8,  CFGPACK_TYPE_I16, CFGPACK_TYPE_I32, CFGPACK_TYPE_I64,
        CFGPACK_TYPE_F32, CFGPACK_TYPE_F64, CFGPACK_TYPE_STR, CFGPACK_TYPE_FSTR,
    };
    size_t n = sizeof(types) / sizeof(types[0]);

    snprintf(schema->map_name, sizeof(schema->map_name), "allty");
    schema->version = 1;
    schema->entry_count = n;
    schema->entries = entries;
    for (size_t i = 0; i < n; ++i) {
        entries[i].index = (uint16_t)(i + 1);
        snprintf(entries[i].name, sizeof(entries[i].name), "t%zu", i);
        entries[i].type = types[i];
        entries[i].has_default = 0;
    }
    return (cfgpack_init(ctx, schema, values, n, str_pool, str_pool_cap,
                         str_offsets, str_offsets_count));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. Typed setters by name — exercise all set_*_by_name wrappers in api.h
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_typed_setters_by_name) {
    LOG_SECTION("All typed set_*_by_name wrappers");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[12];
    cfgpack_value_t values[12];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_all_types_ctx(&ctx, &schema, entries, values, str_pool,
                             sizeof(str_pool), str_offsets, 2) == CFGPACK_OK);

    CHECK(cfgpack_set_u8_by_name(&ctx, "t0", 255) == CFGPACK_OK);
    CHECK(cfgpack_set_u16_by_name(&ctx, "t1", 65535) == CFGPACK_OK);
    CHECK(cfgpack_set_u32_by_name(&ctx, "t2", 0xDEADBEEF) == CFGPACK_OK);
    CHECK(cfgpack_set_u64_by_name(&ctx, "t3", 0xCAFEBABEULL) == CFGPACK_OK);
    CHECK(cfgpack_set_i8_by_name(&ctx, "t4", -128) == CFGPACK_OK);
    CHECK(cfgpack_set_i16_by_name(&ctx, "t5", -32768) == CFGPACK_OK);
    CHECK(cfgpack_set_i32_by_name(&ctx, "t6", -2000000) == CFGPACK_OK);
    CHECK(cfgpack_set_i64_by_name(&ctx, "t7", -9000000000LL) == CFGPACK_OK);
    CHECK(cfgpack_set_f32_by_name(&ctx, "t8", 3.14f) == CFGPACK_OK);
    CHECK(cfgpack_set_f64_by_name(&ctx, "t9", 2.71828) == CFGPACK_OK);
    CHECK(cfgpack_set_str_by_name(&ctx, "t10", "hello") == CFGPACK_OK);
    CHECK(cfgpack_set_fstr_by_name(&ctx, "t11", "bye") == CFGPACK_OK);
    LOG("All 12 typed set_*_by_name calls succeeded");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. Typed getters by index — exercise all get_* wrappers in api.h
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_typed_getters_by_index) {
    LOG_SECTION("All typed get_* wrappers by index");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[12];
    cfgpack_value_t values[12];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_all_types_ctx(&ctx, &schema, entries, values, str_pool,
                             sizeof(str_pool), str_offsets, 2) == CFGPACK_OK);

    /* Set values first */
    CHECK(cfgpack_set_u8(&ctx, 1, 42) == CFGPACK_OK);
    CHECK(cfgpack_set_u16(&ctx, 2, 1000) == CFGPACK_OK);
    CHECK(cfgpack_set_u32(&ctx, 3, 100000) == CFGPACK_OK);
    CHECK(cfgpack_set_u64(&ctx, 4, 999999999ULL) == CFGPACK_OK);
    CHECK(cfgpack_set_i8(&ctx, 5, -50) == CFGPACK_OK);
    CHECK(cfgpack_set_i16(&ctx, 6, -1000) == CFGPACK_OK);
    CHECK(cfgpack_set_i32(&ctx, 7, -100000) == CFGPACK_OK);
    CHECK(cfgpack_set_i64(&ctx, 8, -999999999LL) == CFGPACK_OK);
    CHECK(cfgpack_set_f32(&ctx, 9, 1.5f) == CFGPACK_OK);
    CHECK(cfgpack_set_f64(&ctx, 10, 2.5) == CFGPACK_OK);
    CHECK(cfgpack_set_str(&ctx, 11, "test") == CFGPACK_OK);
    CHECK(cfgpack_set_fstr(&ctx, 12, "fx") == CFGPACK_OK);

    /* Read back with typed getters */
    uint8_t u8_val;
    uint16_t u16_val;
    uint32_t u32_val;
    uint64_t u64_val;
    int8_t i8_val;
    int16_t i16_val;
    int32_t i32_val;
    int64_t i64_val;
    float f32_val;
    double f64_val;

    CHECK(cfgpack_get_u8(&ctx, 1, &u8_val) == CFGPACK_OK);
    CHECK(u8_val == 42);
    CHECK(cfgpack_get_u16(&ctx, 2, &u16_val) == CFGPACK_OK);
    CHECK(u16_val == 1000);
    CHECK(cfgpack_get_u32(&ctx, 3, &u32_val) == CFGPACK_OK);
    CHECK(u32_val == 100000);
    CHECK(cfgpack_get_u64(&ctx, 4, &u64_val) == CFGPACK_OK);
    CHECK(u64_val == 999999999ULL);
    CHECK(cfgpack_get_i8(&ctx, 5, &i8_val) == CFGPACK_OK);
    CHECK(i8_val == -50);
    CHECK(cfgpack_get_i16(&ctx, 6, &i16_val) == CFGPACK_OK);
    CHECK(i16_val == -1000);
    CHECK(cfgpack_get_i32(&ctx, 7, &i32_val) == CFGPACK_OK);
    CHECK(i32_val == -100000);
    CHECK(cfgpack_get_i64(&ctx, 8, &i64_val) == CFGPACK_OK);
    CHECK(i64_val == -999999999LL);
    CHECK(cfgpack_get_f32(&ctx, 9, &f32_val) == CFGPACK_OK);
    CHECK(f32_val == 1.5f);
    CHECK(cfgpack_get_f64(&ctx, 10, &f64_val) == CFGPACK_OK);
    CHECK(f64_val == 2.5);
    LOG("All 12 typed get_* calls verified correct values");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. Typed getter type mismatch — get_u16 on a u8 entry, etc.
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_typed_getter_type_mismatch) {
    LOG_SECTION("Typed getters return TYPE_MISMATCH on wrong type");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[12];
    cfgpack_value_t values[12];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_all_types_ctx(&ctx, &schema, entries, values, str_pool,
                             sizeof(str_pool), str_offsets, 2) == CFGPACK_OK);

    /* Set the u8 entry at index 1 */
    CHECK(cfgpack_set_u8(&ctx, 1, 10) == CFGPACK_OK);

    /* Try reading it with every wrong typed getter */
    uint16_t u16_out;
    uint32_t u32_out;
    uint64_t u64_out;
    int8_t i8_out;
    int16_t i16_out;
    int32_t i32_out;
    int64_t i64_out;
    float f32_out;
    double f64_out;

    CHECK(cfgpack_get_u16(&ctx, 1, &u16_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_u32(&ctx, 1, &u32_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_u64(&ctx, 1, &u64_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_i8(&ctx, 1, &i8_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_i16(&ctx, 1, &i16_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_i32(&ctx, 1, &i32_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_i64(&ctx, 1, &i64_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_f32(&ctx, 1, &f32_out) == CFGPACK_ERR_TYPE_MISMATCH);
    CHECK(cfgpack_get_f64(&ctx, 1, &f64_out) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("All wrong-type getters correctly returned TYPE_MISMATCH");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. cfgpack_presence_clear
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_presence_clear) {
    LOG_SECTION("cfgpack_presence_clear removes entry from present set");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t values[2];
    cfgpack_ctx_t ctx;
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 2);

    CHECK(cfgpack_init(&ctx, &schema, values, 2, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&ctx, 1, 5) == CFGPACK_OK);
    CHECK(cfgpack_presence_get(&ctx, 0) == 1);
    LOG("Entry 0 present after set");

    cfgpack_presence_clear(&ctx, 0);
    CHECK(cfgpack_presence_get(&ctx, 0) == 0);
    LOG("Entry 0 absent after clear");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_ERR_MISSING);
    LOG("get() correctly returns MISSING after presence clear");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. set_by_name type mismatch and str too long
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_by_name_type_mismatch) {
    LOG_SECTION("set_by_name with wrong type -> TYPE_MISMATCH");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t values[2];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 2);
    entries[0].type = CFGPACK_TYPE_U8;
    entries[1].type = CFGPACK_TYPE_STR;

    CHECK(cfgpack_init(&ctx, &schema, values, 2, str_pool, sizeof(str_pool),
                       str_offsets, 1) == CFGPACK_OK);

    /* Try setting u16 on a u8 entry via set_by_name */
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U16, .v.u64 = 42};
    CHECK(cfgpack_set_by_name(&ctx, "e0", &v) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("set_by_name type mismatch: OK");

    /* STR too long via set_by_name */
    char long_str[CFGPACK_STR_MAX + 2];
    memset(long_str, 'x', CFGPACK_STR_MAX + 1);
    long_str[CFGPACK_STR_MAX + 1] = '\0';
    v.type = CFGPACK_TYPE_STR;
    v.v.str.len = CFGPACK_STR_MAX + 1;
    CHECK(cfgpack_set_by_name(&ctx, "e1", &v) == CFGPACK_ERR_STR_TOO_LONG);
    LOG("set_by_name str too long: OK");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. set_by_name with FSTR too long
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_by_name_fstr_too_long) {
    LOG_SECTION("set_by_name with FSTR too long -> STR_TOO_LONG");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;
    char str_pool[32];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    entries[0].type = CFGPACK_TYPE_FSTR;

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 1) == CFGPACK_OK);

    cfgpack_value_t v = {.type = CFGPACK_TYPE_FSTR};
    v.v.fstr.len = CFGPACK_FSTR_MAX + 1;
    CHECK(cfgpack_set_by_name(&ctx, "e0", &v) == CFGPACK_ERR_STR_TOO_LONG);
    LOG("set_by_name fstr too long: OK");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. core.c string getter error paths: type mismatch and missing entry
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_str_getter_errors) {
    LOG_SECTION("String getter error paths in core.c");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_value_t values[3];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[2];

    make_schema(&schema, entries, 3);
    entries[0].type = CFGPACK_TYPE_U8;
    entries[1].type = CFGPACK_TYPE_STR;
    entries[2].type = CFGPACK_TYPE_FSTR;

    CHECK(cfgpack_init(&ctx, &schema, values, 3, str_pool, sizeof(str_pool),
                       str_offsets, 2) == CFGPACK_OK);

    const char *str_out;
    uint16_t str_len;
    uint8_t fstr_len;

    /* get_str on non-str type (u8) -> TYPE_MISMATCH */
    CHECK(cfgpack_set_u8(&ctx, 1, 10) == CFGPACK_OK);
    CHECK(cfgpack_get_str(&ctx, 1, &str_out, &str_len) ==
          CFGPACK_ERR_TYPE_MISMATCH);
    LOG("get_str on u8 entry: TYPE_MISMATCH");

    /* get_fstr on non-fstr type (u8) -> TYPE_MISMATCH */
    CHECK(cfgpack_get_fstr(&ctx, 1, &str_out, &fstr_len) ==
          CFGPACK_ERR_TYPE_MISMATCH);
    LOG("get_fstr on u8 entry: TYPE_MISMATCH");

    /* get_str on unset str entry -> MISSING */
    CHECK(cfgpack_get_str(&ctx, 2, &str_out, &str_len) == CFGPACK_ERR_MISSING);
    LOG("get_str on unset entry: MISSING");

    /* get_fstr on unset fstr entry -> MISSING */
    CHECK(cfgpack_get_fstr(&ctx, 3, &str_out, &fstr_len) ==
          CFGPACK_ERR_MISSING);
    LOG("get_fstr on unset entry: MISSING");

    /* get_str with invalid index -> MISSING */
    CHECK(cfgpack_get_str(&ctx, 99, &str_out, &str_len) == CFGPACK_ERR_MISSING);
    LOG("get_str with bad index: MISSING");

    /* get_fstr with invalid index -> MISSING */
    CHECK(cfgpack_get_fstr(&ctx, 99, &str_out, &fstr_len) ==
          CFGPACK_ERR_MISSING);
    LOG("get_fstr with bad index: MISSING");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. core.c string setter error paths via by-name: type mismatch, missing
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_str_setter_by_name_errors) {
    LOG_SECTION("String setter by-name error paths");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_value_t values[3];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[2];

    make_schema(&schema, entries, 3);
    entries[0].type = CFGPACK_TYPE_U8;
    entries[1].type = CFGPACK_TYPE_STR;
    entries[2].type = CFGPACK_TYPE_FSTR;

    CHECK(cfgpack_init(&ctx, &schema, values, 3, str_pool, sizeof(str_pool),
                       str_offsets, 2) == CFGPACK_OK);

    /* set_str_by_name on non-str type -> TYPE_MISMATCH (via cfgpack_set_str) */
    CHECK(cfgpack_set_str_by_name(&ctx, "e0", "hello") ==
          CFGPACK_ERR_TYPE_MISMATCH);
    LOG("set_str_by_name on u8: TYPE_MISMATCH");

    /* set_fstr_by_name on non-fstr type -> TYPE_MISMATCH */
    CHECK(cfgpack_set_fstr_by_name(&ctx, "e0", "hello") ==
          CFGPACK_ERR_TYPE_MISMATCH);
    LOG("set_fstr_by_name on u8: TYPE_MISMATCH");

    /* set_str_by_name with unknown name -> MISSING */
    CHECK(cfgpack_set_str_by_name(&ctx, "nope", "val") == CFGPACK_ERR_MISSING);
    LOG("set_str_by_name unknown name: MISSING");

    /* set_fstr_by_name with unknown name -> MISSING */
    CHECK(cfgpack_set_fstr_by_name(&ctx, "nope", "val") == CFGPACK_ERR_MISSING);
    LOG("set_fstr_by_name unknown name: MISSING");

    /* set_str_by_name too long */
    char long_str[CFGPACK_STR_MAX + 2];
    memset(long_str, 'a', CFGPACK_STR_MAX + 1);
    long_str[CFGPACK_STR_MAX + 1] = '\0';
    CHECK(cfgpack_set_str_by_name(&ctx, "e1", long_str) ==
          CFGPACK_ERR_STR_TOO_LONG);
    LOG("set_str_by_name too long: STR_TOO_LONG");

    /* set_fstr_by_name too long */
    char long_fstr[CFGPACK_FSTR_MAX + 2];
    memset(long_fstr, 'b', CFGPACK_FSTR_MAX + 1);
    long_fstr[CFGPACK_FSTR_MAX + 1] = '\0';
    CHECK(cfgpack_set_fstr_by_name(&ctx, "e2", long_fstr) ==
          CFGPACK_ERR_STR_TOO_LONG);
    LOG("set_fstr_by_name too long: STR_TOO_LONG");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. String getter by-name error paths
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_str_getter_by_name_errors) {
    LOG_SECTION("String getter by-name error paths");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t values[2];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[2];

    make_schema(&schema, entries, 2);
    entries[0].type = CFGPACK_TYPE_STR;
    entries[1].type = CFGPACK_TYPE_FSTR;

    CHECK(cfgpack_init(&ctx, &schema, values, 2, str_pool, sizeof(str_pool),
                       str_offsets, 2) == CFGPACK_OK);

    const char *out;
    uint16_t str_len;
    uint8_t fstr_len;

    /* get_str_by_name unknown name -> MISSING */
    CHECK(cfgpack_get_str_by_name(&ctx, "nope", &out, &str_len) ==
          CFGPACK_ERR_MISSING);
    LOG("get_str_by_name unknown: MISSING");

    /* get_fstr_by_name unknown name -> MISSING */
    CHECK(cfgpack_get_fstr_by_name(&ctx, "nope", &out, &fstr_len) ==
          CFGPACK_ERR_MISSING);
    LOG("get_fstr_by_name unknown: MISSING");

    /* get_str_by_name on unset entry -> MISSING */
    CHECK(cfgpack_get_str_by_name(&ctx, "e0", &out, &str_len) ==
          CFGPACK_ERR_MISSING);
    LOG("get_str_by_name unset: MISSING");

    /* get_fstr_by_name on unset entry -> MISSING */
    CHECK(cfgpack_get_fstr_by_name(&ctx, "e1", &out, &fstr_len) ==
          CFGPACK_ERR_MISSING);
    LOG("get_fstr_by_name unset: MISSING");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. Typed getters by name — exercise all get_*_by_name wrappers
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_typed_getters_by_name) {
    LOG_SECTION("All typed get_*_by_name wrappers");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[12];
    cfgpack_value_t values[12];
    cfgpack_ctx_t ctx;
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_all_types_ctx(&ctx, &schema, entries, values, str_pool,
                             sizeof(str_pool), str_offsets, 2) == CFGPACK_OK);

    CHECK(cfgpack_set_u8_by_name(&ctx, "t0", 255) == CFGPACK_OK);
    CHECK(cfgpack_set_u16_by_name(&ctx, "t1", 65535) == CFGPACK_OK);
    CHECK(cfgpack_set_u32_by_name(&ctx, "t2", 0xDEADBEEF) == CFGPACK_OK);
    CHECK(cfgpack_set_u64_by_name(&ctx, "t3", 0xCAFEBABEULL) == CFGPACK_OK);
    CHECK(cfgpack_set_i8_by_name(&ctx, "t4", -128) == CFGPACK_OK);
    CHECK(cfgpack_set_i16_by_name(&ctx, "t5", -32768) == CFGPACK_OK);
    CHECK(cfgpack_set_i32_by_name(&ctx, "t6", -2000000) == CFGPACK_OK);
    CHECK(cfgpack_set_i64_by_name(&ctx, "t7", -9000000000LL) == CFGPACK_OK);
    CHECK(cfgpack_set_f32_by_name(&ctx, "t8", 3.14f) == CFGPACK_OK);
    CHECK(cfgpack_set_f64_by_name(&ctx, "t9", 2.71828) == CFGPACK_OK);

    uint16_t u16_val;
    uint32_t u32_val;
    uint64_t u64_val;
    int8_t i8_val;
    int16_t i16_val;
    int32_t i32_val;
    int64_t i64_val;
    float f32_val;
    double f64_val;

    CHECK(cfgpack_get_u16_by_name(&ctx, "t1", &u16_val) == CFGPACK_OK);
    CHECK(u16_val == 65535);
    CHECK(cfgpack_get_u32_by_name(&ctx, "t2", &u32_val) == CFGPACK_OK);
    CHECK(u32_val == 0xDEADBEEF);
    CHECK(cfgpack_get_u64_by_name(&ctx, "t3", &u64_val) == CFGPACK_OK);
    CHECK(u64_val == 0xCAFEBABEULL);
    CHECK(cfgpack_get_i8_by_name(&ctx, "t4", &i8_val) == CFGPACK_OK);
    CHECK(i8_val == -128);
    CHECK(cfgpack_get_i16_by_name(&ctx, "t5", &i16_val) == CFGPACK_OK);
    CHECK(i16_val == -32768);
    CHECK(cfgpack_get_i32_by_name(&ctx, "t6", &i32_val) == CFGPACK_OK);
    CHECK(i32_val == -2000000);
    CHECK(cfgpack_get_i64_by_name(&ctx, "t7", &i64_val) == CFGPACK_OK);
    CHECK(i64_val == -9000000000LL);
    CHECK(cfgpack_get_f32_by_name(&ctx, "t8", &f32_val) == CFGPACK_OK);
    CHECK(f32_val == 3.14f);
    CHECK(cfgpack_get_f64_by_name(&ctx, "t9", &f64_val) == CFGPACK_OK);
    CHECK(f64_val == 2.71828);
    LOG("All 9 typed get_*_by_name calls verified correct values");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. cfgpack_init bounds: values_count < entry_count, max entries
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_init_bounds) {
    LOG_SECTION("cfgpack_init bounds checking");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;
    char str_pool[1];
    uint16_t str_offsets[1];

    /* values_count < entry_count */
    make_schema(&schema, entries, 2);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_ERR_BOUNDS);
    LOG("values_count < entry_count: BOUNDS");

    /* entry_count > MAX_ENTRIES */
    schema.entry_count = CFGPACK_MAX_ENTRIES + 1;
    cfgpack_value_t big_values[1];
    CHECK(cfgpack_init(&ctx, &schema, big_values, CFGPACK_MAX_ENTRIES + 1,
                       str_pool, sizeof(str_pool), str_offsets,
                       0) == CFGPACK_ERR_BOUNDS);
    LOG("entry_count > MAX_ENTRIES: BOUNDS");

    /* str_offsets too small for string entries */
    cfgpack_entry_t str_entries[2];
    cfgpack_value_t str_values[2];
    make_schema(&schema, str_entries, 2);
    str_entries[0].type = CFGPACK_TYPE_STR;
    str_entries[1].type = CFGPACK_TYPE_STR;
    CHECK(cfgpack_init(&ctx, &schema, str_values, 2, str_pool, sizeof(str_pool),
                       str_offsets, 1) == CFGPACK_ERR_BOUNDS);
    LOG("str_offsets too small: BOUNDS");

    /* str_pool too small for entries */
    cfgpack_entry_t one_str[1];
    cfgpack_value_t one_val[1];
    uint16_t one_off[1];
    make_schema(&schema, one_str, 1);
    one_str[0].type = CFGPACK_TYPE_STR;
    CHECK(cfgpack_init(&ctx, &schema, one_val, 1, str_pool, 1, one_off, 1) ==
          CFGPACK_ERR_BOUNDS);
    LOG("str_pool too small: BOUNDS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. get_by_name on entry that exists but is not present
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_get_by_name_not_present) {
    LOG_SECTION("get_by_name on entry not present -> MISSING");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    cfgpack_value_t v;
    CHECK(cfgpack_get_by_name(&ctx, "e0", &v) == CFGPACK_ERR_MISSING);
    LOG("get_by_name on valid but unset entry: MISSING");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 12. io_file: measure_file with nonexistent path
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_measure_file_nonexistent) {
    LOG_SECTION("measure_file with nonexistent path -> ERR_IO");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    char scratch[256];

    CHECK(cfgpack_schema_measure_file("/tmp/cfgpack_no_such_file.map", &m,
                                      scratch, sizeof(scratch),
                                      &err) == CFGPACK_ERR_IO);
    CHECK(err.line == 0);
    LOG("measure_file nonexistent: IO, err.message='%s'", err.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 13. io_file: measure_json_file with nonexistent path
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_measure_json_file_nonexistent) {
    LOG_SECTION("measure_json_file with nonexistent path -> ERR_IO");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    char scratch[256];

    CHECK(cfgpack_schema_measure_json_file("/tmp/cfgpack_no_such.json", &m,
                                           scratch, sizeof(scratch),
                                           &err) == CFGPACK_ERR_IO);
    CHECK(err.line == 0);
    LOG("measure_json_file nonexistent: IO, err.message='%s'", err.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 14. io_file: parse_schema_file with nonexistent path
 * ═════════════════════════════════════════════════════════════��═════════════ */
TEST_CASE(test_parse_schema_file_nonexistent) {
    LOG_SECTION("parse_schema_file with nonexistent path -> ERR_IO");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    cfgpack_parse_error_t err;
    char scratch[256];

    cfgpack_parse_opts_t opts = {
        .out_schema = &schema,
        .entries = entries,
        .max_entries = 4,
        .values = values,
        .err = &err,
    };

    CHECK(cfgpack_parse_schema_file("/tmp/cfgpack_no_such.map", &opts, scratch,
                                    sizeof(scratch)) == CFGPACK_ERR_IO);
    CHECK(err.line == 0);
    LOG("parse_schema_file nonexistent: IO, err.message='%s'", err.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 15. io_file: read_file overflow (file larger than scratch)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_file_scratch_overflow) {
    LOG_SECTION("File functions with tiny scratch buffer -> ERR_IO");

    /* Create a file larger than our tiny scratch */
    const char *path = "build/coverage_test_big.map";
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "test 1\n");
    for (int i = 1; i <= 20; i++) {
        fprintf(f, "%d e%d u8 0\n", i, i);
    }
    fclose(f);

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    char tiny_scratch[8];

    CHECK(cfgpack_schema_measure_file(path, &m, tiny_scratch,
                                      sizeof(tiny_scratch),
                                      &err) == CFGPACK_ERR_IO);
    LOG("measure_file with tiny scratch: IO (file too big)");

    /* Also test measure_json_file with tiny scratch on a real file */
    const char *json_path = "build/coverage_test.json";
    f = fopen(json_path, "w");
    CHECK(f != NULL);
    fprintf(f, "{\"name\":\"test\",\"version\":1,\"entries\":[]}");
    fclose(f);

    CHECK(cfgpack_schema_measure_json_file(json_path, &m, tiny_scratch,
                                           sizeof(tiny_scratch),
                                           &err) == CFGPACK_ERR_IO);
    LOG("measure_json_file with tiny scratch: IO (file too big)");

    /* Binary read overflow: pagein_file with 1-byte scratch on a real file */
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_ctx_t ctx;
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    /* Write a binary file larger than 1 byte */
    const char *bin_path = "build/coverage_test.bin";
    f = fopen(bin_path, "wb");
    CHECK(f != NULL);
    uint8_t data[16] = {0x81, 0x01, 0x05, 0, 0, 0, 0, 0,
                        0,    0,    0,    0, 0, 0, 0, 0};
    fwrite(data, 1, sizeof(data), f);
    fclose(f);

    uint8_t tiny_bin[1];
    CHECK(cfgpack_pagein_file(&ctx, bin_path, tiny_bin, sizeof(tiny_bin)) ==
          CFGPACK_ERR_IO);
    LOG("pagein_file with 1-byte scratch on 16-byte file: IO");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 16. io_file: write_json_file with tiny scratch -> ERR_BOUNDS
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_write_json_file_tiny_scratch) {
    LOG_SECTION("write_json_file with tiny scratch -> ERR_BOUNDS");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t values[2];
    cfgpack_ctx_t ctx;
    char str_pool[128];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t err;

    make_schema(&schema, entries, 2);
    entries[1].type = CFGPACK_TYPE_STR;

    CHECK(cfgpack_init(&ctx, &schema, values, 2, str_pool, sizeof(str_pool),
                       str_offsets, 1) == CFGPACK_OK);
    CHECK(cfgpack_set_u8(&ctx, 1, 42) == CFGPACK_OK);
    CHECK(cfgpack_set_str(&ctx, 2, "hello world") == CFGPACK_OK);

    char tiny[4];
    cfgpack_err_t rc = cfgpack_schema_write_json_file(
        &ctx, "build/coverage_wont_write.json", tiny, sizeof(tiny), &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("write_json_file with 4-byte scratch: BOUNDS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 17. io_file: measure_json_file and parse_json_file happy path
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_measure_and_parse_json_file) {
    LOG_SECTION("measure_json_file + schema_parse_json_file roundtrip");

    /* Write a valid JSON schema file */
    const char *path = "build/coverage_json_schema.json";
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "{\n"
               "  \"name\": \"jsont\",\n"
               "  \"version\": 1,\n"
               "  \"entries\": [\n"
               "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", "
               "\"value\": 10},\n"
               "    {\"index\": 2, \"name\": \"b\", \"type\": \"i16\", "
               "\"value\": -5}\n"
               "  ]\n"
               "}\n");
    fclose(f);

    /* Measure */
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    char scratch[1024];

    CHECK(cfgpack_schema_measure_json_file(path, &m, scratch, sizeof(scratch),
                                           &err) == CFGPACK_OK);
    CHECK(m.entry_count == 2);
    LOG("measure_json_file: %zu entries", m.entry_count);

    /* Parse */
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    cfgpack_parse_opts_t opts = {
        .out_schema = &schema,
        .entries = entries,
        .max_entries = 4,
        .values = values,
        .err = &err,
    };

    CHECK(cfgpack_schema_parse_json_file(path, &opts, scratch,
                                         sizeof(scratch)) == CFGPACK_OK);
    CHECK(schema.entry_count == 2);
    CHECK(schema.version == 1);
    CHECK(strcmp(schema.map_name, "jsont") == 0);
    LOG("schema_parse_json_file: name='%s' version=%u entries=%zu",
        schema.map_name, schema.version, schema.entry_count);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 18. io_file: pageout_file with tiny scratch -> ERR_ENCODE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_pageout_file_tiny_scratch) {
    LOG_SECTION("pageout_file with tiny scratch -> ERR_ENCODE");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t values[2];
    cfgpack_ctx_t ctx;
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 2);
    CHECK(cfgpack_init(&ctx, &schema, values, 2, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    CHECK(cfgpack_set_u8(&ctx, 1, 42) == CFGPACK_OK);
    CHECK(cfgpack_set_u8(&ctx, 2, 99) == CFGPACK_OK);

    uint8_t tiny[1];
    CHECK(cfgpack_pageout_file(&ctx, "build/coverage_wont_write.bin", tiny,
                               sizeof(tiny)) == CFGPACK_ERR_ENCODE);
    LOG("pageout_file with 1-byte scratch: ENCODE");

    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("typed_setters_by_name",
                                 test_typed_setters_by_name()) != TEST_OK);
    overall |= (test_case_result("typed_getters_by_index",
                                 test_typed_getters_by_index()) != TEST_OK);
    overall |= (test_case_result("typed_getter_type_mismatch",
                                 test_typed_getter_type_mismatch()) != TEST_OK);
    overall |= (test_case_result("presence_clear", test_presence_clear()) !=
                TEST_OK);
    overall |= (test_case_result("set_by_name_type_mismatch",
                                 test_set_by_name_type_mismatch()) != TEST_OK);
    overall |= (test_case_result("set_by_name_fstr_too_long",
                                 test_set_by_name_fstr_too_long()) != TEST_OK);
    overall |= (test_case_result("str_getter_errors",
                                 test_str_getter_errors()) != TEST_OK);
    overall |= (test_case_result("str_setter_by_name_errors",
                                 test_str_setter_by_name_errors()) != TEST_OK);
    overall |= (test_case_result("str_getter_by_name_errors",
                                 test_str_getter_by_name_errors()) != TEST_OK);
    overall |= (test_case_result("typed_getters_by_name",
                                 test_typed_getters_by_name()) != TEST_OK);
    overall |= (test_case_result("init_bounds", test_init_bounds()) != TEST_OK);
    overall |= (test_case_result("get_by_name_not_present",
                                 test_get_by_name_not_present()) != TEST_OK);
    overall |= (test_case_result("measure_file_nonexistent",
                                 test_measure_file_nonexistent()) != TEST_OK);
    overall |= (test_case_result("measure_json_file_nonexistent",
                                 test_measure_json_file_nonexistent()) !=
                TEST_OK);
    overall |= (test_case_result("parse_schema_file_nonexistent",
                                 test_parse_schema_file_nonexistent()) !=
                TEST_OK);
    overall |= (test_case_result("file_scratch_overflow",
                                 test_file_scratch_overflow()) != TEST_OK);
    overall |= (test_case_result("write_json_file_tiny_scratch",
                                 test_write_json_file_tiny_scratch()) !=
                TEST_OK);
    overall |= (test_case_result("measure_and_parse_json_file",
                                 test_measure_and_parse_json_file()) !=
                TEST_OK);
    overall |= (test_case_result("pageout_file_tiny_scratch",
                                 test_pageout_file_tiny_scratch()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
