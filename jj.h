#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-reserved-identifier"
#ifndef JJ_H
#define JJ_H

#include <ctype.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.c/hashmap.h"

typedef struct hashmap hashmap;

typedef uint16_t jj_valtype;

typedef int64_t jj_jsontype_int;
typedef char* jj_jsontype_str;
typedef long double jj_jsontype_float;
typedef bool jj_jsontype_bool;

#define JJ_JSON_NULL  NULL
#define JJ_JSON_TRUE  true
#define JJ_JSON_FALSE false

#define JJ_VALTYPE_NULL  114514
#define JJ_VALTYPE_OBJ   1
#define JJ_VALTYPE_ARR   2
#define JJ_VALTYPE_INT   3
#define JJ_VALTYPE_FLOAT 4
#define JJ_VALTYPE_BOOL  5
#define JJ_VALTYPE_STR   6

typedef union _jsondata;

// if name is NULL, then it's root
typedef struct _jsonobj;

typedef struct _jsonarrdata {
    uint32_t length;
    uint32_t cap;
    struct _jsonobj* arr;
} jj_jsonarrdata;

typedef union _jsondata {
    jj_jsontype_int intval;
    jj_jsontype_str strval;
    jj_jsontype_float floatval;
    jj_jsontype_bool boolval;
    hashmap* objval;
    struct _jsonarrdata* arrval;
} jj_jsondata;

typedef struct _jsonobj {
    jj_valtype type;
    char* name;

    union _jsondata data;
} jj_jsonobj;

static inline uint64_t _jj_hash_func(const void* item, uint64_t seed0,
                                     uint64_t seed1) {
    const jj_jsonobj* j = (const jj_jsonobj*)item;
    return hashmap_sip(j->name, strlen(j->name), seed0, seed1);
}

static inline int _jj_comp_func(const void* a, const void* b, void* udata) {
    return (int)((size_t)a - (size_t)b);
}

static inline hashmap* _jj_new_hashmap() {
    return hashmap_new(sizeof(jj_jsonobj), 0, 0, 0, _jj_hash_func,
                       _jj_comp_func, NULL, NULL);
}

static inline void _jj_hashmap_free(hashmap* map) { hashmap_free(map); }

// don't free subelements of json structure. only free the root.
void jj_free(jj_jsonobj* root);
inline void _jj_arrfree(jj_jsonarrdata* arr);

static inline int _jj_arrresize(jj_jsonarrdata* arr, size_t size) {
    jj_jsonobj* loc = realloc(arr->arr, size * sizeof(jj_jsonobj));
    if (!loc) {
        return -1;
    }
    arr->arr = loc;
    arr->cap = size;
    return 0;
}

inline int _jj_arrappend(jj_jsonarrdata* arr, jj_jsonobj* obj) {
    if (arr->length + 1 >= arr->cap) {
        if (_jj_arrresize(arr, (arr->length + 1) << 1) != 0) {
            return -1;
        }
    }
    memcpy(arr->arr + arr->length, obj, sizeof(jj_jsonobj));
    arr->length++;
    return 0;
}

#define _JJ_MALLOC_NEW_JSONOBJ()             \
    (jj_jsonobj*)malloc(sizeof(jj_jsonobj)); \
    if (!val) {                              \
        return NULL;                         \
    }

inline bool jj_is_json_root(jj_jsonobj* json);
inline bool jj_is_json_type(jj_jsonobj* json, jj_valtype type);
inline void jj_setname(jj_jsonobj* json, const char* name);

#define _JJ_JSONOBJ_INIT_SETNAME(val, name) \
    (val)->name = NULL;                     \
    if (name) jj_setname((val), (name));

// if name is NULL then it's root
inline jj_jsonobj* jj_new_empty_obj(const char* name);
inline jj_jsonobj* jj_new_jsonbool(const char* name, jj_jsontype_bool b);
inline jj_jsonobj* jj_new_jsonint(const char* name, jj_jsontype_int i);
inline jj_jsonobj* jj_new_jsonfloat(const char* name, jj_jsontype_float f);
inline jj_jsonobj* jj_new_jsonstr(const char* name, jj_jsontype_str s);
inline jj_jsonobj* jj_new_jsonnull(const char* name);
inline jj_jsonobj* jj_new_jsonobj(const char* name);
inline void jj_jsonobj_put(jj_jsonobj* obj, jj_jsonobj* val);
inline jj_jsonobj* jj_oget(jj_jsonobj* obj, const char* name);
inline jj_jsonobj* jj_aget(jj_jsonobj* obj, uint32_t idx);
// returns true if success, false if not of type bool.
inline bool jj_ogetbool(jj_jsonobj* obj, const char* name,
                        jj_jsontype_bool* result);

// ***************************** parsing **********************************

typedef uint16_t _jj_token_type;

#define _JJ_TOKEN_INT     1001
#define _JJ_TOKEN_FLOAT   1002
#define _JJ_TOKEN_STR     1003
#define _JJ_TOKEN_NULL    1004
#define _JJ_TOKEN_TRUE    1005
#define _JJ_TOKEN_FALSE   1006
#define _JJ_TOKEN_INVALID 1145
#define _JJ_TOKEN_EOF     1146

typedef struct _jj_lexstate {
    const char* original;
    uint32_t length;

    size_t cur_idx;
    size_t line;
    size_t col;
    char* strbuf;
    size_t buflen;
    size_t bufcap;
    _jj_token_type curtoken;
} _jj_lexstate;

static inline bool _jj_is_char_oneof(_jj_token_type c, const char* chars) {
    if (c > 255) {
        return false;
    }
    for (int i = 0;; i++) {
        if (chars[i] == '\0') {
            break;
        }
        if (c == chars[i]) {
            return true;
        }
    }
    return false;
}

// c cannot be NULL char (0).
static inline bool _jj_str_contains_s(const char* s, size_t len, char c) {
    if (c == 0) {
        return false;
    }
    for (int i = 0; i < len; i++) {
        if (s[i] == 0) {
            return false;
        }
        if (s[i] == c) {
            return true;
        }
    }
    return false;
}

static inline bool _jj_str_isint(const char* s, const size_t len,
                                 bool* isinvalid) {
    size_t i = 0;
    *isinvalid = false;
    if (s[i] == '-') i++;
    if (s[i] == '0') {
        *isinvalid = true;
        return false;
    }
    for (; i < len; i++) {
        if (s[i] == 0) return true;
        if (!isdigit(s[i])) return false;
    }
    return true;
}

static inline _jj_lexstate* _jj_new_lexstate(const char* original,
                                             const uint32_t length) {
    _jj_lexstate* s = malloc(sizeof(_jj_lexstate));
    if (!s) {
        return NULL;
    }
    s->original = original;
    s->length = length;
    s->line = 1;
    s->col = 1;
    s->buflen = 0;
    s->bufcap = 15;
    s->strbuf = malloc(s->bufcap);
    s->cur_idx = 0;
    s->curtoken = 0;
    return s;
}

static inline void _jj_free_lexstate(_jj_lexstate* s) {
    free(s->strbuf);
    free(s);
}

static inline void _jj_lexstate_nextchar(_jj_lexstate* s) {
    if (s->original[s->cur_idx] == '\n') s->line++;
    s->cur_idx++;
    s->col++;
}

static inline bool _jj_lexstate_isatend(_jj_lexstate* s) {
    return s->cur_idx >= s->length;
}

static inline bool _jj_lexstate_bufequals(_jj_lexstate* s, const char* other,
                                          size_t cnt) {
    return s->buflen == cnt && strncmp(s->strbuf, other, cnt) == 0;
}

static inline void _jj_lexstate_resize_strbuf(_jj_lexstate* s) {
    if (s->buflen + 1 >= s->bufcap) {
        size_t cap = (s->buflen + 1) << 1;
        char* new = realloc(s->strbuf, cap);
        if (!new) {
            exit(1145141919810ULL);
        }
        s->strbuf = new;
        s->bufcap = cap;
    }
}

static inline void _jj_lexstate_reset_strbuf(_jj_lexstate* s) { s->buflen = 0; }

static inline void _jj_lexstate_append_strbuf(_jj_lexstate* s, char c) {
    _jj_lexstate_resize_strbuf(s);
    s->strbuf[s->buflen] = c;
    s->buflen++;
}

static inline void _jj_lexstate_err(_jj_lexstate* s) {
    if (s->curtoken < 256) {
        fprintf(stderr, "Invalid char '%c' at line %zu col %zu\n", s->curtoken,
                s->line, s->col);
        return;
    }
    _jj_lexstate_resize_strbuf(s);
    s->strbuf[s->buflen] = '\0';
    s->buflen++;
    fprintf(stderr, "Invalid token '%s' at line %zu col %zu\n", s->strbuf,
            s->line, s->col);
}

#define _JJ_PARSE_ERR(state)  \
    _jj_lexstate_err(state);  \
    _jj_free_lexstate(state); \
    return NULL

void _jj_lex_next(_jj_lexstate* state);
jj_jsonobj* jj_parse(const char* json_str, uint32_t length);

#endif  // JJ_H
#pragma clang diagnostic pop