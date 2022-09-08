#ifndef JJ_H
#define JJ_H

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
typedef char *jj_jsontype_str;
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
    struct _jsonobj *arr;
} jj_jsonarrdata;

typedef union _jsondata {
    jj_jsontype_int intval;
    jj_jsontype_str strval;
    jj_jsontype_float floatval;
    jj_jsontype_bool boolval;
    hashmap *objval;
    struct _jsonarrdata *arrval;
} jj_jsondata;

typedef struct _jsonobj {
    jj_valtype type;
    char *name;

    union _jsondata data;
} jj_jsonobj;

uint64_t _jj_hash_func(const void *item, uint64_t seed0, uint64_t seed1) {
    const jj_jsonobj *j = (const jj_jsonobj *)item;
    return hashmap_sip(j->name, strlen(j->name), seed0, seed1);
}

int _jj_comp_func(const void *a, const void *b, void *udata) {
    return (int)((size_t)a - (size_t)b);
}

inline hashmap *_jj_new_hashmap() {
    return hashmap_new(sizeof(jj_jsonobj), 0, 0, 0, _jj_hash_func,
                       _jj_comp_func, NULL, NULL);
}

inline void _jj_hashmap_free(hashmap *map) { hashmap_free(map); }

inline void _jj_arrfree(jj_jsonarrdata *arr) {
    jj_free(arr->arr);
    jj_free(arr);
}

inline int _jj_arrappend(jj_jsonarrdata *arr, jj_jsonobj *obj) {
    if (arr->length + 1 >= arr->cap) {
        if (_jj_arrresize(arr, (arr->length + 1) << 1) != 0) {
            return -1;
        }
    }
    memcpy(arr->arr + arr->length, obj, sizeof(jj_jsonobj));
    arr->length++;
    return 0;
}

inline int _jj_arrresize(jj_jsonarrdata *arr, size_t size) {
    jj_jsonobj *loc = realloc(arr->arr, size * sizeof(jj_jsonobj));
    if (!loc) {
        return -1;
    }
    arr->arr = loc;
    arr->cap = size;
    return 0;
}

#define _JJ_MALLOC_NEW_JSONOBJ()              \
    (jj_jsonobj *)malloc(sizeof(jj_jsonobj)); \
    if (!val) {                               \
        return NULL;                          \
    }

#define _JJ_JSONOBJ_INIT_SETNAME(val, name) \
    (val)->name = NULL;                     \
    if (name) jj_setname((val), (name));

inline bool jj_is_json_root(jj_jsonobj *json) { return json->name == NULL; }

inline bool jj_is_json_type(jj_jsonobj *json, jj_valtype type) {
    return json->type == type;
}

inline void jj_setname(jj_jsonobj *json, const char *name) {
    free(json->name);
    if (!name) {
        json->name = NULL;
        return;
    }
    size_t len = strlen(name);
    char *buf = malloc(len);
    strcpy(buf, name);
    json->name = buf;
}

// if name is NULL then it's root
inline jj_jsonobj *jj_new_empty_obj(const char *name) {
    jj_jsonobj *val = _JJ_MALLOC_NEW_JSONOBJ();
    _JJ_JSONOBJ_INIT_SETNAME(val, name);
    return val;
}

inline jj_jsonobj *jj_new_jsonbool(const char *name, jj_jsontype_bool b) {
    jj_jsonobj *val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_BOOL;
    val->data.boolval = b;
    return val;
}

inline jj_jsonobj *jj_new_jsonint(const char *name, jj_jsontype_int i) {
    jj_jsonobj *val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_INT;
    val->data.intval = i;
    return val;
}

inline jj_jsonobj *jj_new_jsonfloat(const char *name, jj_jsontype_float f) {
    jj_jsonobj *val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_FLOAT;
    val->data.floatval = f;
    return val;
}

inline jj_jsonobj *jj_new_jsonnull(const char *name) {
    jj_jsonobj *val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_NULL;
    return val;
}

inline jj_jsonobj *jj_new_jsonobj(const char *name) {
    jj_jsonobj *val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_OBJ;
    val->data.objval = _jj_new_hashmap();
    return val;
}

inline void jj_jsonobj_put(jj_jsonobj *obj, jj_jsonobj *val) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return;
    }
    hashmap_set(obj->data.objval, val);
}

// don't free subelements of json structure. only free the root.
void jj_free(jj_jsonobj *root) {
    if (root->name) free(root->name);
    switch (root->type) {
        case JJ_VALTYPE_STR:
            free(root->data.strval);
            break;
        case JJ_VALTYPE_OBJ:
            _jj_hashmap_free(root->data.objval);
            break;
        case JJ_VALTYPE_ARR:
            _jj_arrfree(root->data.arrval);
            break;
        default:
            break;
    }
    free(root);
}

inline jj_jsonobj *jj_oget(jj_jsonobj *obj, const char *name) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return NULL;
    }
    return hashmap_get(obj->data.objval, &(jj_jsonobj){.name = name});
}

inline jj_jsonobj *jj_aget(jj_jsonobj *obj, uint32_t idx) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_ARR)) {
        return NULL;
    }
    return obj->data.arrval->arr + idx;
}

// returns true if success, false if not of type bool.
inline bool jj_ogetbool(jj_jsonobj *obj, const char *name,
                        jj_jsontype_bool *result) {
    jj_jsonobj *ref = jj_oget(obj, name);
    if (!ref) {
        return false;
    }
    if (!jj_is_json_type(ref, JJ_VALTYPE_BOOL)) {
        return false;
    }
    *result = ref->data.boolval;
    return true;
}

// ***************************** parsing **********************************

typedef uint16_t _jj_token_type;

#define _JJ_TOKEN_INT     1001
#define _JJ_TOKEN_FLOAT   1002
#define _JJ_TOKEN_STR     1003
#define _JJ_TOKEN_NULL    1004
#define _JJ_TOKEN_TRUE    1005
#define _JJ_TOKEN_FALSE   1006
#define _JJ_TOKEN_INVALID 114514
#define _JJ_TOKEN_EOF     114515

typedef struct _jj_lexstate {
    const char *original;
    const uint32_t length;

    size_t cur_idx;
    size_t line;
    size_t col;
    char *strbuf;
    size_t buflen;
    size_t bufcap;
    _jj_token_type curtoken;
} _jj_lexstate;

inline bool _jj_is_char_oneof(_jj_token_type c, const char *chars) {
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
inline bool _jj_str_contains_s(const char *s, size_t len, char c) {
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

inline _jj_lexstate *_jj_new_lexstate() {
    _jj_lexstate *s = malloc(sizeof(_jj_lexstate));
    if (!s) {
        return NULL;
    }
    s->line = 1;
    s->col = 1;
    s->buflen = 0;
    s->bufcap = 15;
    s->strbuf = malloc(s->bufcap);
    s->cur_idx = 0;
    s->curtoken = 0;
    return s;
}

inline void _jj_free_lexstate(_jj_lexstate *s) {
    free(s->strbuf);
    free(s);
}

inline void _jj_lexstate_nextchar(_jj_lexstate *s) {
    s->cur_idx++;
    s->col++;
}

inline void _jj_lexstate_resize_strbuf(_jj_lexstate *s) {
    if (s->buflen + 1 >= s->bufcap) {
        size_t cap = (s->buflen + 1) << 1;
        char *new = realloc(s->strbuf, cap);
        if (!new) {
            exit(1145141919810ULL);
        }
        s->strbuf = new;
        s->bufcap = cap;
    }
}

inline void __jj_lexstate_reset_strbuf(_jj_lexstate *s) { s->buflen = 0; }

inline void _jj_lexstate_err(_jj_lexstate *s) {
    if (s->curtoken < 256) {
        fprintf(stderr, "Invalid char '%c' at line %d col %d\n", s->curtoken,
                s->line, s->col);
        return;
    }
    _jj_lexstate_resize_strbuf(s);
    s->strbuf[s->buflen] = '\0';
    s->buflen++;
    fprintf(stderr, "Invalid token '%s' at line %d col %d\n", s->strbuf,
            s->line, s->col);
}

void _jj_lex_next(_jj_lexstate *state);
jj_jsonobj *jj_parse(const char *json_str);

#endif  // JJ_H