#ifndef JJ_H
#define JJ_H

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charvec.h"
#include "hashmap.c/hashmap.h"

typedef struct hashmap hashmap;

typedef uint16_t jj_valtype;

typedef int64_t jj_jsontype_int;
typedef char* jj_jsontype_str;
typedef long double jj_jsontype_float;
typedef bool jj_jsontype_bool;

#if defined(__GNUC__) || defined(__clang__)
#define UJJ_MAYBE_UNUSED __attribute__((unused))
#else
#define UJJ_MAYBE_UNUSED
#endif

#if !(defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || \
      defined(__NT__))
#define strcpy_s(dest, destlen, src)       strcpy((dest), (src))
#define strncpy_s(dest, destlen, src, len) strncpy((dest), (src), (len))
#endif

#define JJ_JSON_TRUE  true
#define JJ_JSON_FALSE false

#define JJ_VALTYPE_NULL  1145
#define JJ_VALTYPE_OBJ   1
#define JJ_VALTYPE_ARR   2
#define JJ_VALTYPE_INT   3
#define JJ_VALTYPE_FLOAT 4
#define JJ_VALTYPE_BOOL  5
#define JJ_VALTYPE_STR   6

typedef union jj_jsondata jj_jsondata;
// if name is NULL, then it's root
typedef struct jj_jsonobj jj_jsonobj;
typedef struct jj_jsonarrdata jj_jsonarrdata;
typedef struct jj_tostr_config jj_tostr_config;

struct jj_jsonarrdata {
    uint32_t length;
    uint32_t cap;
    struct jj_jsonobj* arr;
};

union jj_jsondata {
    UJJ_MAYBE_UNUSED jj_jsontype_int intval;
    UJJ_MAYBE_UNUSED jj_jsontype_str strval;
    UJJ_MAYBE_UNUSED jj_jsontype_float floatval;
    UJJ_MAYBE_UNUSED jj_jsontype_bool boolval;
    UJJ_MAYBE_UNUSED hashmap* objval;
    UJJ_MAYBE_UNUSED struct jj_jsonarrdata* arrval;
};

struct jj_jsonobj {
    jj_valtype type;
    char* name;

    union jj_jsondata data;
};

struct jj_tostr_config {
    int indent;     /** number of spaces for indentation when formatted */
    bool sp;        /** if space is needed when formatted is false */
    bool formatted; /** if formatted */
};

// don't free subelements of json structure. only free the root.
void ojj_free_innerval(jj_jsonobj* obj);
void jj_free(jj_jsonobj* root);
void ojj_objfree(hashmap* root);
void ojj_arrfree(jj_jsonarrdata* arr);

static uint64_t ojj_hm_hash_func(const void* item, uint64_t seed0,
                                 uint64_t seed1) {
    const jj_jsonobj* j = (const jj_jsonobj*)item;
    return hashmap_sip(j->name, strlen(j->name), seed0, seed1);
}

static int ojj_hm_comp_func(const void* a, const void* b,
                            UJJ_MAYBE_UNUSED void* udata) {
    const jj_jsonobj* j1 = (const jj_jsonobj*)a;
    const jj_jsonobj* j2 = (const jj_jsonobj*)b;
    return strcmp(j1->name, j2->name);
}

static void ojj_hm_free_func(void* v) {
    jj_jsonobj* j = (jj_jsonobj*)v;
    ojj_free_innerval(j);
}

static inline hashmap* ojj_new_hashmap() {
    return hashmap_new(sizeof(jj_jsonobj), 0, 0, 0, ojj_hm_hash_func,
                       ojj_hm_comp_func, ojj_hm_free_func, NULL);
}

static inline void ojj_hashmap_free(hashmap* map) { hashmap_free(map); }

static inline jj_jsonarrdata* ojj_newarrdata(size_t cap) {
    jj_jsonarrdata* arrdata = malloc(sizeof(jj_jsonarrdata));
    arrdata->length = 0;
    arrdata->cap = cap;
    arrdata->arr = malloc(sizeof(jj_jsonobj) * cap);
    return arrdata;
}

static inline int ojj_arrresize(jj_jsonarrdata* arr, size_t size) {
    jj_jsonobj* new = realloc(arr->arr, size * sizeof(jj_jsonobj));
    if (!new) {
        return -1;
    }
    arr->arr = new;
    arr->cap = size;
    return 0;
}

// takes ownership of obj.
static inline int ojj_arrappend(jj_jsonarrdata* arr, jj_jsonobj* obj) {
    if (!obj) {
        return -1;
    }
    if (arr->length + 1 >= arr->cap) {
        if (ojj_arrresize(arr, (arr->length + 1) << 1) != 0) {
            return -1;
        }
    }
    memcpy(arr->arr + arr->length, obj, sizeof(jj_jsonobj));
    arr->length++;
    free(obj);  // since I make a shallow copy of it
    return 0;
}

// returns a newly allocated string
static inline jj_jsontype_str ujj_clonestr(jj_jsontype_str orig, size_t len) {
    jj_jsontype_str s = malloc(len + 1);
    if (!s) return NULL;
    strncpy_s(s, len + 1, orig, len);
    s[len] = 0;
    return s;
}

#define OJJ_MALLOC_NEW_JSONOBJ()             \
    (jj_jsonobj*)malloc(sizeof(jj_jsonobj)); \
    if (!val) {                              \
        return NULL;                         \
    }

// ***************************** exposed apis *****************************

UJJ_MAYBE_UNUSED static inline bool jj_is_json_root(jj_jsonobj* json) {
    return json->name == NULL;
}

UJJ_MAYBE_UNUSED static inline bool jj_is_json_type(jj_jsonobj* json,
                                                    jj_valtype type) {
    if (!json) return false;
    return json->type == type;
}

// Sets name for a json object. Creates a new copy of `name`.
static inline void jj_setname(jj_jsonobj* json, const char* const name) {
    free(json->name);
    if (!name) {
        json->name = NULL;
        return;
    }
    size_t len = strlen(name);
    char* buf = malloc(len + 1);
    strcpy_s(buf, len + 1, name);
    json->name = buf;
}

#define OJJ_GENFUNC_NEWOBJ(ty, valtype)                         \
    UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_json##ty( \
        const char* name, jj_jsontype_##ty v) {                 \
        jj_jsonobj* obj = jj_new_empty_obj(name);               \
        obj->type = valtype;                                    \
        obj->data.ty##val = v;                                  \
        return obj;                                             \
    }

#define OJJ_JSONOBJ_INIT_SETNAME(val, name) \
    (val)->name = NULL;                     \
    if (name) jj_setname((val), (name));

// if name is NULL then it's root
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_empty_obj(const char* name) {
    jj_jsonobj* val = (jj_jsonobj*)malloc(sizeof(jj_jsonobj));
    if (!val) {
        return NULL;
    }
    OJJ_JSONOBJ_INIT_SETNAME(val, name);
    return val;
}

OJJ_GENFUNC_NEWOBJ(bool, JJ_VALTYPE_BOOL)
OJJ_GENFUNC_NEWOBJ(int, JJ_VALTYPE_INT)
OJJ_GENFUNC_NEWOBJ(float, JJ_VALTYPE_FLOAT)
// Takes ownership of `s`. If you want to put a copy of the string in the
// new object, use `jj_new_jsonstr` instead.
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_jsonstrref(
    const char* name, jj_jsontype_str v) {
    jj_jsonobj* obj = jj_new_empty_obj(name);
    obj->type = JJ_VALTYPE_STR;
    obj->data.strval = v;
    return obj;
}
// Copies `s` with specific length. If you want the new object to take ownership
// of the string, use `jj_new_jsonstrref` instead.
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_jsonstrn(const char* name,
                                                           jj_jsontype_str v,
                                                           size_t len) {
    jj_jsontype_str s = ujj_clonestr(v, len);
    return jj_new_jsonstrref(name, s);
}
// Copies `s`. If you want the new object to take ownership of the string,
// use `jj_new_jsonstrref` instead.
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_jsonstr(const char* name,
                                                          jj_jsontype_str v) {
    return jj_new_jsonstrn(name, v, strlen(v));
}
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_jsonnull(const char* name) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_NULL;
    return val;
}
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_jsonobj(const char* name) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_OBJ;
    val->data.objval = ojj_new_hashmap();
    return val;
};
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_new_jsonarr(const char* name) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_ARR;
    val->data.arrval = ojj_newarrdata(3);
    return val;
};

// takes ownership of `val`
UJJ_MAYBE_UNUSED void jj_oput(jj_jsonobj* obj, jj_jsonobj* val);
UJJ_MAYBE_UNUSED jj_jsonobj* jj_oget(jj_jsonobj* obj, const char* name);

UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_aget(jj_jsonobj* obj,
                                                   uint32_t idx) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_ARR)) {
        return NULL;
    }
    return obj->data.arrval->arr + idx;
}

#define OJJ_GENFUNC_AGET_ASTYPE(type, valtype)                       \
    UJJ_MAYBE_UNUSED static inline bool jj_aget##type(               \
        jj_jsonobj* obj, uint32_t idx, jj_jsontype_##type* result) { \
        jj_jsonobj* ref = jj_aget(obj, idx);                         \
        if (!jj_is_json_type(ref, valtype)) {                        \
            return false;                                            \
        }                                                            \
        *result = ref->data.type##val;                               \
        return true;                                                 \
    }

OJJ_GENFUNC_AGET_ASTYPE(int, JJ_VALTYPE_INT)
OJJ_GENFUNC_AGET_ASTYPE(float, JJ_VALTYPE_FLOAT)
OJJ_GENFUNC_AGET_ASTYPE(bool, JJ_VALTYPE_BOOL)
// returns a reference of string for this object, or NULL if not of type string.
UJJ_MAYBE_UNUSED static inline jj_jsontype_str jj_agetstrref(jj_jsonobj* obj,
                                                             uint32_t idx) {
    jj_jsonobj* ref = jj_aget(obj, idx);
    if (!jj_is_json_type(ref, JJ_VALTYPE_STR)) {
        return NULL;
    }
    return ref->data.strval;
}
// returns a newly allocated string, or NULL if failed.
// If you just want a reference, use `jj_agetstrref` instead.
UJJ_MAYBE_UNUSED static inline jj_jsontype_str jj_agetstr(jj_jsonobj* obj,
                                                          uint32_t idx) {
    jj_jsontype_str s = jj_agetstrref(obj, idx);
    return ujj_clonestr(s, strlen(s));
}

// Takes ownership of val and all its children. Return true if success, false if
// obj is not array or append fails
UJJ_MAYBE_UNUSED static inline bool jj_aappend(jj_jsonobj* obj,
                                               jj_jsonobj* val) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_ARR)) {
        return false;
    }
    return ojj_arrappend(obj->data.arrval, val) == 0;
}

UJJ_MAYBE_UNUSED static inline bool jj_isnull(jj_jsonobj* obj) {
    return jj_is_json_type(obj, JJ_VALTYPE_NULL);
}

#define OJJ_GENFUNC_OGET_ASTYPE(type, valtype)                           \
    UJJ_MAYBE_UNUSED static inline bool jj_oget##type(                   \
        jj_jsonobj* obj, const char* name, jj_jsontype_##type* result) { \
        jj_jsonobj* ref = jj_oget(obj, name);                            \
        if (!jj_is_json_type(ref, valtype)) {                            \
            return false;                                                \
        }                                                                \
        *result = ref->data.type##val;                                   \
        return true;                                                     \
    }

// returns true if success, false if not of type bool.
OJJ_GENFUNC_OGET_ASTYPE(bool, JJ_VALTYPE_BOOL)
// returns true if success, false if not of type int.
OJJ_GENFUNC_OGET_ASTYPE(int, JJ_VALTYPE_INT)
// returns true if success, false if not of type float.
OJJ_GENFUNC_OGET_ASTYPE(float, JJ_VALTYPE_FLOAT)
// returns a reference of string for this object, or NULL if not of type string.
UJJ_MAYBE_UNUSED static inline jj_jsontype_str jj_ogetstrref(jj_jsonobj* obj,
                                                             const char* name) {
    jj_jsonobj* ref = jj_oget(obj, name);
    if (!jj_is_json_type(ref, JJ_VALTYPE_STR)) {
        return NULL;
    }
    return ref->data.strval;
}
// returns a newly allocated string, or NULL if failed.
// If you just want a reference, use `jj_ogetstrref` instead.
UJJ_MAYBE_UNUSED static inline jj_jsontype_str jj_ogetstr(jj_jsonobj* obj,
                                                          const char* name) {
    jj_jsontype_str s = jj_ogetstrref(obj, name);
    return ujj_clonestr(s, strlen(s));
}
UJJ_MAYBE_UNUSED static inline jj_jsonobj* jj_ogetarridx(jj_jsonobj* obj,
                                                         const char* name,
                                                         size_t idx) {
    jj_jsonobj* a = jj_oget(obj, name);
    if (!jj_is_json_type(a, JJ_VALTYPE_ARR)) {
        return NULL;
    }
    return jj_aget(a, idx);
}

// ***************************** parsing *****************************

typedef uint16_t ljj_token_type;

#define LJJ_TOKEN_INT     0x0101
#define LJJ_TOKEN_FLOAT   0x0201
#define LJJ_TOKEN_STR     0x0401
#define LJJ_TOKEN_NULL    0x0801
#define LJJ_TOKEN_TRUE    0x1001
#define LJJ_TOKEN_FALSE   0x2001
#define LJJ_TOKEN_INVALID 0x4000
#define LJJ_TOKEN_EOF     0x8000

typedef struct ljj_lexstate {
    ljj_token_type curtoken;
    bool instr;
    uint32_t length;
    const char* original;

    size_t cur_idx;
    size_t line;
    size_t col;
    // char* strbuf;
    // size_t buflen;
    // size_t bufcap;
    charvec* strbuf;
} ljj_lexstate;

#define LJJ_LEXSTATE_CURCHAR(state) (state)->original[(state)->cur_idx]

#define LJJ_LEXSTATE_ISINVALID(state) (((state)->curtoken & 0xC000))

static inline bool ujj_is_char_oneof(ljj_token_type c, const char* chars) {
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
static inline bool ujj_str_contains_s(const char* s, size_t len, char c) {
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

static inline bool ujj_str_isjsonint(const char* s, const size_t len) {
    size_t i = 0;
    if (s[i] == '-') i++;
    if (len - i > 19) {
        return false;
    }
    if (s[i] == '0' && len != 1) {  // if not a single 0
        return false;
    }
    // for (; i < len; i++) {
    //     if (s[i] == 0) return true;
    //     if (!isdigit(s[i])) return false;
    // }
    char* pend;
    strtoll(s, &pend, 10);
    if (errno == ERANGE) {
        return false;
    }
    if (pend != s + len) {
        return false;
    }
    return true;
}

static inline bool ujj_str_isvalidjsonfloat(const char* s, const size_t len) {
    size_t i = 0;
    if (s[i] == '-') i++;
    while (i < len && isdigit(s[i])) {
        i++;
    }
    if (s[i] == '.') {
        i++;
        while (i < len && isdigit(s[i])) {
            i++;
        }
    }
    if (i == len) {
        return true;
    }
    if (!ujj_is_char_oneof(s[i], "eE")) {
        return false;
    }
    i++;
    if (ujj_is_char_oneof(s[i], "-+")) {
        i++;
    }
    while (i < len && isdigit(s[i])) {
        i++;
    }
    return i == len;
}

static inline bool ujj_unicode_to_utf8(charvec* strbuf, int c) {
    if (c < 0x80)
        charvec_append(strbuf, (char)c);
    else if (c < 0x800) {
        charvec_append(strbuf, (char)(192 + c / 64));
        charvec_append(strbuf, (char)(128 + c % 64));
    } else if (c - 0xd800u < 0x800) {
        return false;
    } else if (c < 0x10000) {
        charvec_append(strbuf, (char)(224 + c / 4096));
        charvec_append(strbuf, (char)(128 + c / 64 % 64));
        charvec_append(strbuf, (char)(128 + c % 64));
    } else if (c < 0x110000) {
        charvec_append(strbuf, (char)(240 + c / 262144));
        charvec_append(strbuf, (char)(128 + c / 4096 % 64));
        charvec_append(strbuf, (char)(128 + c / 64 % 64));
        charvec_append(strbuf, (char)(128 + c % 64));
    } else {
        return false;
    }
    return true;
}

static inline ljj_lexstate* ljj_new_lexstate(const char* original,
                                             const uint32_t length) {
    ljj_lexstate* s = malloc(sizeof(ljj_lexstate));
    if (!s) {
        return NULL;
    }
    s->original = original;
    s->length = length;
    s->line = 1;
    s->col = 1;
    // s->buflen = 0;
    // s->bufcap = 15;
    // s->strbuf = malloc(s->bufcap);
    s->strbuf = charvec_new(15);
    s->cur_idx = 0;
    s->curtoken = 0;
    return s;
}

static inline void ljj_free_lexstate(ljj_lexstate* s) {
    charvec_free(s->strbuf);
    free(s);
}

static inline void ljj_lexstate_nextchar(ljj_lexstate* s) {
    if (!s->instr && s->original[s->cur_idx] == '\n') {
        s->line++;
        s->col = 0;
    }
    s->cur_idx++;
    s->col++;
}

static inline bool ljj_lexstate_isatend(ljj_lexstate* s) {
    return s->cur_idx >= s->length;
}

static inline size_t ljj_lexstate_buflen(ljj_lexstate* s) {
    return charvec_len(s->strbuf);
}

static inline size_t ljj_lexstate_bufcap(ljj_lexstate* s) {
    return s->strbuf->cap;
}

static inline char ljj_lexstate_getc(ljj_lexstate* s, size_t idx) {
    return charvec_get(s->strbuf, idx);
}

static inline char* ljj_lexstate_buf(ljj_lexstate* s) { return s->strbuf->buf; }

static inline bool ljj_lexstate_bufequals(ljj_lexstate* s, const char* other,
                                          size_t cnt) {
    // return s->buflen == cnt && strncmp(s->strbuf, other, cnt) == 0;
    return ljj_lexstate_buflen(s) == cnt &&
           strncmp(ljj_lexstate_buf(s), other, cnt) == 0;
}

// static inline void ljj_lexstate_resize_strbuf(ljj_lexstate* s) {
//     if (s->buflen + 1 >= s->bufcap) {
//         size_t cap = (s->buflen + 1) << 1;
//         char* new = realloc(s->strbuf, cap);
//         if (!new) {
//             exit(114514);
//         }
//         s->strbuf = new;
//         s->bufcap = cap;
//     }
// }

static inline void ljj_lexstate_clear_strbuf(ljj_lexstate* s) {
    charvec_clear(s->strbuf);
}

static inline void ljj_lexstate_append_strbuf(ljj_lexstate* s, char c) {
    // ljj_lexstate_resize_strbuf(s);
    // s->strbuf[s->buflen] = c;
    // s->buflen++;
    charvec_append(s->strbuf, c);
}

static inline void ljj_lexstate_err(ljj_lexstate* s) {
    if (s->curtoken & LJJ_TOKEN_EOF) {
        fprintf(stderr, "Encountered EOF\n");
        return;
    }
    char tok = (char)(s->curtoken & 0xFF);
    if ((s->curtoken & LJJ_TOKEN_INVALID) && tok > 0) {
        fprintf(stderr, "Invalid token '%c' at line %zu col %zu\n", tok,
                s->line, s->col - 1);
        return;
    }
    ljj_lexstate_append_strbuf(s, '\0');
    fprintf(stderr, "Invalid token '%s' at line %zu col %zu\n", s->strbuf->buf,
            s->line, s->col - ljj_lexstate_buflen(s) + 1);
}

#define LJJ_LEXSTATE_ERR(state) \
    ljj_lexstate_err(state);    \
    ljj_free_lexstate(state);   \
    return NULL

static void ljj_lex_skip_whitespace(ljj_lexstate* state) {
    char cur;
    while (true) {
        if (ljj_lexstate_isatend(state)) {
            state->curtoken = LJJ_TOKEN_EOF;
            return;
        }
        cur = LJJ_LEXSTATE_CURCHAR(state);
        if (ujj_is_char_oneof(cur, " \r\t\n")) {
            ljj_lexstate_nextchar(state);
            continue;
        }
        break;
    }
}

void ljj_lex_read_str(ljj_lexstate* state);
void ljj_lex_read_val(ljj_lexstate* state);
void ljj_lex_next(ljj_lexstate* state);
// returns a newly allocated string, or null if not able to
jj_jsontype_str ljj_lexstate_getstr(ljj_lexstate* state);
bool ljj_lexstate_getint(ljj_lexstate* state, jj_jsontype_int* result);
bool ljj_lexstate_getfloat(ljj_lexstate* state, jj_jsontype_float* result);
jj_jsonobj* ljj_lexstate_parseobj(ljj_lexstate* state, const char* name);
jj_jsonobj* ljj_lexstate_parsearr(ljj_lexstate* state, const char* name);

void sjj_tostr_jobj(charvec* strbuf, jj_jsondata data, int depth,
                    jj_tostr_config* config, bool inarr);
int sjj_tostr_jdata(jj_jsonobj* obj, charvec* strbuf, int depth,
                    jj_tostr_config* config, bool inarr);
int sjj_tostr(jj_jsonobj* obj, charvec* strbuf, int depth,
              jj_tostr_config* config, bool inarr);

jj_jsonobj* jj_parse(const char* json_str, uint32_t length);
UJJ_MAYBE_UNUSED char* jj_tostr(jj_jsonobj* obj, jj_tostr_config* config);

#endif  // JJ_H
