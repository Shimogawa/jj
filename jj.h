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

typedef int64_t json_val_int;
typedef char* json_val_str;
typedef long double json_val_float;
typedef bool json_val_bool;

#define JSON_NULL NULL
#define JSON_TRUE true
#define JSON_FALSE false

#define JJ_VALTYPE_NULL 114514
#define JJ_VALTYPE_OBJ 1
#define JJ_VALTYPE_ARR 2
#define JJ_VALTYPE_INT 3
#define JJ_VALTYPE_FLOAT 4
#define JJ_VALTYPE_BOOL 5
#define JJ_VALTYPE_STR 6

typedef union _jsondata;

// if name is NULL, then it's root
typedef struct _jsonobj;

typedef struct _jsonarrdata {
    uint32_t length;
    uint32_t cap;
    struct _jsonobj* arr;
} jj_jsonarrdata;

typedef union _jsondata {
    json_val_int intval;
    json_val_str strval;
    json_val_float floatval;
    json_val_bool boolval;
    hashmap* objval;
    struct _jsonarrdata* arrval;
} jj_jsondata;

typedef struct _jsonobj {
    jj_valtype type;
    char* name;

    union _jsondata data;
} jj_jsonobj;

uint64_t __jj_hash_func(const void* item, uint64_t seed0, uint64_t seed1) {
    const jj_jsonobj* j = (const jj_jsonobj*)item;
    return hashmap_sip(j->name, strlen(j->name), seed0, seed1);
}

int __jj_comp_func(const void* a, const void* b, void* udata) { return a - b; }

inline hashmap* __jj_new_hashmap() {
    return hashmap_new(sizeof(jj_jsonobj), 0, 0, 0, __jj_hash_func,
                       __jj_comp_func, NULL, NULL);
}

inline void __jj_hashmap_free(hashmap* map) { hashmap_free(map); }

inline void __jj_arrfree(jj_jsonarrdata* arr) {
    jj_free(arr->arr);
    jj_free(arr);
}

inline int __jj_arrappend(jj_jsonarrdata* arr, jj_jsonobj* obj) {
    if (arr->length + 1 >= arr->cap) {
        if (__jj_arrresize(arr, (arr->length + 1) << 1) != 0) {
            return -1;
        }
    }
    memcpy(arr->arr + arr->length, obj, sizeof(jj_jsonobj));
    arr->length++;
    return 0;
}

inline int __jj_arrresize(jj_jsonarrdata* arr, size_t size) {
    jj_jsonobj* loc = realloc(arr->arr, size * sizeof(jj_jsonobj));
    if (!loc) {
        return -1;
    }
    arr->arr = loc;
    arr->cap = size;
    return 0;
}

#define __JJ_MALLOC_NEW_JSONOBJ()                              \
    jj_jsonobj* val = (jj_jsonobj*)malloc(sizeof(jj_jsonobj)); \
    if (!val) {                                                \
        return NULL;                                           \
    }

inline bool jj_is_json_root(jj_jsonobj* json) { return json->name == NULL; }

inline bool jj_is_json_type(jj_jsonobj* json, jj_valtype type) {
    return json->type == type;
}

inline void jj_setname(jj_jsonobj* json, const char* name) {
    free(json->name);
    size_t len = strlen(name);
    char* buf = malloc(len);
    strcpy(buf, name);
    json->name = buf;
}

inline jj_jsonobj* jj_new_jsonbool(const char* name, bool b) {
    __JJ_MALLOC_NEW_JSONOBJ();
    val->type = JJ_VALTYPE_BOOL;
    val->data.boolval = b;
    return val;
}

inline jj_jsonobj* jj_new_jsonobj(const char* name) {
    __JJ_MALLOC_NEW_JSONOBJ();
    val->type = JJ_VALTYPE_OBJ;
    val->name = NULL;
    jj_setname(val, name);
    val->data.objval = __jj_new_hashmap();
    return val;
}

inline void jj_jsonobj_put(jj_jsonobj* obj, jj_jsonobj* val) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return;
    }
    hashmap_set(obj->data.objval, val);
}

void jj_free(jj_jsonobj* root) {
    free(root->name);
    switch (root->type) {
        case JJ_VALTYPE_INT:
        case JJ_VALTYPE_FLOAT:
        case JJ_VALTYPE_BOOL:
            free(root);
            break;
        case JJ_VALTYPE_STR:
            free(root->data.strval);
            break;
        case JJ_VALTYPE_OBJ:
            __jj_hashmap_free(root->data.objval);
            break;
        case JJ_VALTYPE_ARR:
            __jj_arrfree(root->data.arrval);
            break;
        default:
            break;
    }
}

typedef uint16_t __jj_token_type;

#define __JJ_TOKEN_INT 1001
#define __JJ_TOKEN_FLOAT 1002
#define __JJ_TOKEN_STR 1003
#define __JJ_TOKEN_NULL 1004
#define __JJ_TOKEN_INVALID 114514

typedef struct __jj_lexstate {
    const char* original;
    const uint32_t length;

    size_t cur_idx;
    size_t line;
    size_t col;
    char* strbuf;
    size_t buflen;
    size_t bufcap;
    __jj_token_type curtoken;
} __jj_lexstate;

inline bool __jj_is_char_oneof(__jj_token_type c, const char* chars) {
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
inline bool __jj_str_contains_s(const char* s, size_t len, char c) {
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

inline __jj_lexstate* __jj_new_lexstate() {
    __jj_lexstate* s = malloc(sizeof(__jj_lexstate));
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

inline void __jj_free_lexstate(__jj_lexstate* s) {
    free(s->strbuf);
    free(s);
}

inline void __jj_lexstate_nextchar(__jj_lexstate* s) {
    s->cur_idx++;
    s->col++;
}

inline void __jj_lexstate_resize_strbuf(__jj_lexstate* s) {
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

inline void __jj_lexstate_reset_strbuf(__jj_lexstate* s) { s->buflen = 0; }

inline void __jj_lexstate_err(__jj_lexstate* s) {
    if (s->curtoken < 256) {
        fprintf(stderr, "Invalid char '%c' at line %d col %d\n", s->curtoken,
                s->line, s->col);
        return;
    }
    __jj_lexstate_resize_strbuf(s);
    s->strbuf[s->buflen] = '\0';
    s->buflen++;
    fprintf(stderr, "Invalid token '%s' at line %d col %d\n", s->strbuf,
            s->line, s->col);
}

void __jj_lex_next(__jj_lexstate* state);
jj_jsonobj* jj_parse(const char* json_str);

#endif  // JJ_H