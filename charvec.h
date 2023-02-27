#ifndef CHARVEC_H
#define CHARVEC_H

#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

#define newp(type)    ((type*)malloc(sizeof(type)))
#define newn(type, n) ((type*)malloc(sizeof(type) * (n)))

#define CHARVEC_MAX(a, b) ((a) >= (b) ? (a) : (b))

#define CHARVEC_THE_DEFAULT 0
#define CHARVEC_MAXSIZE     (SIZE_MAX - 100)

typedef struct charvec {
    char* buf;
    size_t len;
    size_t cap;
} charvec;

static inline charvec* charvec_new(size_t cap) {
    charvec* v = newp(charvec);
    if (!v) {
        return NULL;
    }
    v->cap = cap;
    v->len = 0;
    v->buf = newn(char, cap);
    if (!v->buf) {
        free(v);
        return NULL;
    }
    return v;
}

static void charvec_free(charvec* v) {
    free(v->buf);
    free(v);
}

static inline int i_charvec_resize(charvec* v, size_t tgtlen) {
    if (tgtlen * 4 < v->cap * 3) {  // if tgtlen < 3/4 cap
        return 0;
    }
    // else set cap = 2 * max(tgtlen, cap)
    size_t tgtcap = CHARVEC_MAX(tgtlen, v->cap) << 1;
    if (tgtcap < v->cap) {  // if overflow
        if (v->cap == CHARVEC_MAXSIZE) {
            return 0;
        }
        tgtcap = CHARVEC_MAXSIZE;
    }
    char* tmpbuf = (char*)realloc(v->buf, tgtcap * sizeof(char));
    if (!tmpbuf) {
        return 1;
    }
    v->buf = tmpbuf;
    v->cap = tgtcap;
    return 0;
}

static inline int i_charvec_checkbound(charvec* v, size_t len) {
    return len >= v->cap;
}

// return 1 if fail to allocate new space for vector, 2 if no space available,
// otherwise 0
inline static int charvec_append(charvec* v, char c) {
    if (i_charvec_resize(v, v->len + 1)) {  // if err
        return 1;
    }
    if (i_charvec_checkbound(v, v->len + 1)) {
        return 2;
    }
    v->buf[v->len++] = c;
    return 0;
}

// return 1 if fail to allocate new space for vector, 2 if no space available,
// otherwise 0
inline static int charvec_appendn(charvec* v, char* p, size_t n) {
    if (i_charvec_resize(v, v->len + n)) {
        return 1;
    }
    size_t newlen = v->len + n;
    if (i_charvec_checkbound(v, newlen)) {
        return 2;
    }
    memcpy(v->buf + v->len, p, n * sizeof(char));
    v->len = newlen;
    return 0;
}

// returns default value (0) if out of bound.
inline static char charvec_get(charvec* v, size_t idx) {
    if (idx > v->len) {
        return CHARVEC_THE_DEFAULT;
    }
    return v->buf[idx];
}

inline static void charvec_clear(charvec* v) { v->len = 0; }

inline static size_t charvec_len(charvec* v) { return v->len; }

// returns a newly malloced array copy of the vector.
inline static char* charvec_asarr(charvec* v) {
    char* buf = newn(char, v->len);
    if (!buf) {
        return NULL;
    }
    memcpy(buf, v->buf, v->len * sizeof(char));
    return buf;
}

// returns the ref of the buffer and frees the object
inline static char* charvec_takeownership(charvec* v, size_t* o_len) {
    char* buf = v->buf;
    if (o_len) *o_len = v->len;
    free(v);
    return buf;
}

// creates a newly allocated NULL terminated cstring.
static inline char* charvec_tostr(charvec* v) {
    char* s = newn(char, v->len + 1);
    if (!s) {
        return NULL;
    }
    memcpy(s, v->buf, v->len * sizeof(char));
    s[v->len] = 0;
    return s;
}

#endif  // CHARVEC_H