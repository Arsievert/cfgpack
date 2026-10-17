/**
 * @file parse_error_str.c
 * @brief Human-readable text for cfgpack_parse_code_t.
 *
 * Hosted-only translation unit.  It is intentionally excluded from the core
 * library so that the string table costs nothing in an embedded build; tools,
 * tests and examples compile it in alongside the archive.
 */

#include "cfgpack/schema.h"

const char *cfgpack_parse_error_str(cfgpack_parse_code_t code) {
    switch (code) {
    case CFGPACK_PARSE_OK: return "no error";
    case CFGPACK_PARSE_FILE_OPEN: return "unable to open file";
    case CFGPACK_PARSE_SCRATCH_TOO_SMALL: return "scratch buffer too small";
    case CFGPACK_PARSE_BUFFER_TOO_SMALL: return "buffer too small";
    case CFGPACK_PARSE_LINE_TOO_LONG: return "line too long";
    case CFGPACK_PARSE_MISSING_HEADER: return "missing header";
    case CFGPACK_PARSE_INVALID_HEADER: return "invalid header";
    case CFGPACK_PARSE_MAP_NAME_TOO_LONG: return "map name too long";
    case CFGPACK_PARSE_INVALID_VERSION: return "invalid version";
    case CFGPACK_PARSE_VERSION_OUT_OF_RANGE: return "version out of range";
    case CFGPACK_PARSE_TOO_MANY_ENTRIES: return "too many entries";
    case CFGPACK_PARSE_INVALID_ENTRY: return "invalid entry";
    case CFGPACK_PARSE_INVALID_INDEX: return "invalid index";
    case CFGPACK_PARSE_INDEX_OUT_OF_RANGE: return "index out of range";
    case CFGPACK_PARSE_RESERVED_INDEX:
        return "index 0 is reserved for schema name";
    case CFGPACK_PARSE_INVALID_NAME: return "invalid name";
    case CFGPACK_PARSE_INVALID_ENTRY_NAME: return "invalid entry name";
    case CFGPACK_PARSE_NAME_TOO_LONG: return "name too long";
    case CFGPACK_PARSE_INVALID_TYPE: return "invalid type";
    case CFGPACK_PARSE_DUPLICATE: return "duplicate";
    case CFGPACK_PARSE_MISSING_DEFAULT: return "missing default value";
    case CFGPACK_PARSE_INVALID_DEFAULT: return "invalid default value";
    case CFGPACK_PARSE_TRUNCATED_DEFAULT: return "truncated default value";
    case CFGPACK_PARSE_INVALID_STRING_DEFAULT: return "invalid string default";
    case CFGPACK_PARSE_DEFAULT_OUT_OF_RANGE: return "default out of range";
    case CFGPACK_PARSE_FLOAT_FOR_INT: return "float default for integer type";
    case CFGPACK_PARSE_NUMERIC_FOR_STRING:
        return "default value is numeric but type is string";
    case CFGPACK_PARSE_STRING_FOR_NUMERIC:
        return "default value is string but type is numeric";
    case CFGPACK_PARSE_STR_TOO_LONG: return "str too long";
    case CFGPACK_PARSE_FSTR_TOO_LONG: return "fstr too long";
    case CFGPACK_PARSE_STR_POOL_TOO_SMALL: return "string pool too small";
    case CFGPACK_PARSE_STR_POOL_OFFSET_RANGE:
        return "string pool exceeds offset range";
    case CFGPACK_PARSE_STR_OFFSETS_TOO_SMALL:
        return "str_offsets array too small";
    case CFGPACK_PARSE_MISSING_FIELD: return "missing required field";
    case CFGPACK_PARSE_MISSING_ENTRY_FIELD: return "missing entry field";
    case CFGPACK_PARSE_UNKNOWN_KEY: return "unknown key";
    case CFGPACK_PARSE_UNKNOWN_ENTRY_KEY: return "unknown entry key";
    case CFGPACK_PARSE_INVALID_VALUE: return "invalid value";
    case CFGPACK_PARSE_INVALID_TOP_LEVEL: return "invalid top-level value";
    case CFGPACK_PARSE_EXPECTED_LBRACE: return "expected '{'";
    case CFGPACK_PARSE_EXPECTED_RBRACE: return "expected '}'";
    case CFGPACK_PARSE_EXPECTED_LBRACKET: return "expected '['";
    case CFGPACK_PARSE_EXPECTED_RBRACKET: return "expected ']'";
    case CFGPACK_PARSE_EXPECTED_COLON: return "expected ':'";
    case CFGPACK_PARSE_EXPECTED_KEY: return "expected key";
    case CFGPACK_PARSE_EXPECTED_ENTRY_KEY: return "expected entry key";
    case CFGPACK_PARSE_EXPECTED_UINT_KEY: return "expected uint key";
    case CFGPACK_PARSE_EXPECTED_ENTRY_MAP: return "expected entry map";
    case CFGPACK_PARSE_EXPECTED_ENTRIES_ARRAY:
        return "expected array for entries";
    case CFGPACK_PARSE_EXPECTED_TOP_LEVEL_MAP:
        return "invalid msgpack: expected top-level map";
    case CFGPACK_PARSE_PHASE2_DECODE: return "phase2 decode error";
    case CFGPACK_PARSE_UNEXPECTED_TOKEN: return "unexpected token";
    default: return "unknown parse error";
    }
}
