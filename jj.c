#include "jj.h"

void jj_oput(jj_jsonobj* obj, jj_jsonobj* val) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return;
    }
    hashmap_set(obj->data.objval, val);
    free(val);  // since hashmap makes a shallow copy
}

jj_jsonobj* jj_oget(jj_jsonobj* obj, const char* name) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return NULL;
    }
    jj_jsonobj* g =
        hashmap_get(obj->data.objval, &(jj_jsonobj){.name = (char*)name});
    return g;
}

void ojj_free_innerval(jj_jsonobj* obj) {
    if (!obj) return;
    if (obj->name) free(obj->name);
    switch (obj->type) {
        case JJ_VALTYPE_STR:
            free(obj->data.strval);
            break;
        case JJ_VALTYPE_OBJ:
            ojj_objfree(obj->data.objval);
            break;
        case JJ_VALTYPE_ARR:
            ojj_arrfree(obj->data.arrval);
            break;
        default:
            break;
    }
}

void jj_free(jj_jsonobj* root) {  // NOLINT
    ojj_free_innerval(root);
    free(root);
}

void ojj_objfree(hashmap* hm) {
    if (!hm) return;
    // size_t iter = 0;
    // void* item;
    // while (hashmap_iter(hm, &iter, &item)) {
    //     jj_jsonobj* obj = item;
    //     ojj_free_innerval(obj);
    // }
    hashmap_free(hm);
}

void ojj_arrfree(jj_jsonarrdata* arr) {
    for (uint32_t i = 0; i < arr->length; i++) {
        ojj_free_innerval(arr->arr + i);
    }
    free(arr->arr);
    free(arr);
}

static void ljj_lex_read_str(ljj_lexstate* state) {
    ljj_lexstate_clear_strbuf(state);
    ljj_lexstate_nextchar(state);  // consume '"'
    char cur;
    while (true) {
        if (ljj_lexstate_isatend(state)) {
            state->curtoken = LJJ_TOKEN_INVALID;
            return;
        }
        cur = state->original[state->cur_idx];
        ljj_lexstate_nextchar(state);
        if (ujj_is_char_oneof(cur, "\r\n")) {
            state->curtoken = LJJ_TOKEN_INVALID;
            return;
        }
        if (cur == '"') {
            break;
        }
        // TODO: parse escapes
        ljj_lexstate_append_strbuf(state, cur);
    }
    state->curtoken = LJJ_TOKEN_STR;
}

static void ljj_lex_read_val(ljj_lexstate* state) {
    ljj_lexstate_clear_strbuf(state);
    char cur;
    while (true) {
        if (ljj_lexstate_isatend(state)) {
            break;
        }
        cur = LJJ_LEXSTATE_CURCHAR(state);
        if (ujj_is_char_oneof(cur, " \t\r\n}],")) {
            break;
        }
        if ((cur < '0' || cur > 'z') && cur != '+' && cur != '-' &&
            cur != '.') {
            state->curtoken = LJJ_TOKEN_INVALID;
            return;
        }
        ljj_lexstate_append_strbuf(state, cur);
        ljj_lexstate_nextchar(state);
    }
    if (ljj_lexstate_buflen(state) == 0) {
        state->curtoken = LJJ_TOKEN_INVALID;
        return;
    }
    if (ljj_lexstate_getc(state, 0) == '+') {
        state->curtoken = LJJ_TOKEN_INVALID;
        return;
    }
    if (ljj_lexstate_bufequals(state, "null", 4)) {
        state->curtoken = LJJ_TOKEN_NULL;
        return;
    }
    if (ljj_lexstate_bufequals(state, "true", 4)) {
        state->curtoken = LJJ_TOKEN_TRUE;
        return;
    }
    if (ljj_lexstate_bufequals(state, "false", 5)) {
        state->curtoken = LJJ_TOKEN_FALSE;
        return;
    }
    char* b = charvec_tostr(state->strbuf);
    if (ujj_str_isjsonint(b, ljj_lexstate_buflen(state))) {
        state->curtoken = LJJ_TOKEN_INT;
        free(b);
        return;
    }
    free(b);
    if (ujj_str_isvalidjsonfloat(ljj_lexstate_buf(state),
                                 ljj_lexstate_buflen(state))) {
        state->curtoken = LJJ_TOKEN_FLOAT;
        return;
    }
    state->curtoken = LJJ_TOKEN_INVALID;
}

static void ljj_lex_next(ljj_lexstate* state) {
    ljj_lex_skip_whitespace(state);
    if (state->curtoken == LJJ_TOKEN_EOF) {
        return;
    }
    char cur = LJJ_LEXSTATE_CURCHAR(state);
    if (ujj_is_char_oneof(cur, "{}:,[]")) {
        state->curtoken = (uint8_t)cur;
        ljj_lexstate_nextchar(state);
        return;
    }
    if (cur == '"') {
        ljj_lex_read_str(state);
        return;
    }
    ljj_lex_read_val(state);
}

static jj_jsontype_str ljj_lexstate_getstr(ljj_lexstate* state) {
    // return ujj_clonestr(ljj_lexstate_buf(state), ljj_lexstate_buflen(state));
    return charvec_tostr(state->strbuf);
}

static bool ljj_lexstate_getint(ljj_lexstate* state, jj_jsontype_int* result) {
    char* buf = ljj_lexstate_getstr(state);
    if (!buf) return false;
    char* pend;
    jj_jsontype_int res = strtoll(buf, &pend, 10);
    if (errno == ERANGE || pend != buf + ljj_lexstate_buflen(state)) {
        free(buf);
        return false;
    }
    free(buf);
    *result = res;
    return true;
}

bool ljj_lexstate_getfloat(ljj_lexstate* state, jj_jsontype_float* result) {
    char* buf = ljj_lexstate_getstr(state);
    char* pend;
    jj_jsontype_float res = strtold(buf, &pend);
    if (pend != buf + ljj_lexstate_buflen(state)) {
        free(buf);
        return false;
    }
    *result = res;
    free(buf);
    return true;
}

// If `name` is NULL, then returns a new node. If `name` is not NULL, then
// add this object to the property `name` in `root`, and return `root`.
jj_jsonobj* ljj_lexstate_parsenode(ljj_lexstate* state,  // NOLINT
                                   jj_jsonobj* root, const char* name) {
    jj_jsonobj* new = NULL;
    switch (state->curtoken) {
        case LJJ_TOKEN_NULL:
            new = jj_new_jsonnull(name);
            break;
        case LJJ_TOKEN_TRUE:
            new = jj_new_jsonbool(name, JJ_JSON_TRUE);
            break;
        case LJJ_TOKEN_FALSE:
            new = jj_new_jsonbool(name, JJ_JSON_FALSE);
            break;
        case LJJ_TOKEN_INT: {
            jj_jsontype_int res;
            if (!ljj_lexstate_getint(state, &res)) {
                state->curtoken = LJJ_TOKEN_INVALID;
                return NULL;
            }
            new = jj_new_jsonint(name, res);
            break;
        }
        case LJJ_TOKEN_FLOAT: {
            jj_jsontype_float res;
            if (!ljj_lexstate_getfloat(state, &res)) {
                state->curtoken = LJJ_TOKEN_INVALID;
                return NULL;
            }
            new = jj_new_jsonfloat(name, res);
            break;
        }
        case LJJ_TOKEN_STR: {
            jj_jsontype_str s = ljj_lexstate_getstr(state);
            new = jj_new_jsonstrref(name, s);
            break;
        }
        case '{': {
            new = ljj_lexstate_parseobj(state, name);
            break;
        }
        case '[': {
            new = ljj_lexstate_parsearr(state, name);
            break;
        }
        default: {
            break;
        }
    }
    if (LJJ_LEXSTATE_ISINVALID(state)) {
        return NULL;
    }
    if (!new) {
        state->curtoken |= LJJ_TOKEN_INVALID;
        return NULL;
    }
    if (!name) {
        return new;
    }
    jj_oput(root, new);
    return root;
}

static jj_jsonobj* ljj_lexstate_parseobj(ljj_lexstate* state,  // NOLINT
                                         const char* name) {
    jj_jsonobj* root = jj_new_jsonobj(name);
    char* propname = NULL;
    while (true) {
        free(propname);
        ljj_lex_next(state);
        if (state->curtoken == '}') {
            break;
        }
        if (state->curtoken != LJJ_TOKEN_STR) {
            state->curtoken = LJJ_TOKEN_INVALID;
            free(root);
            return NULL;
        }
        propname = ljj_lexstate_getstr(state);
        if (!propname) {
            free(root);
            return NULL;
        }
        ljj_lex_next(state);
        if (state->curtoken != ':') {
            break;
        }
        ljj_lex_next(state);
        ljj_lexstate_parsenode(state, root, propname);
        if (LJJ_LEXSTATE_ISINVALID(state)) {
            break;
        }
        ljj_lex_next(state);
        if (state->curtoken != ',') {
            break;
        }
    }
    free(propname);
    if (state->curtoken != '}') {
        free(root);
        if (!LJJ_LEXSTATE_ISINVALID(state))
            state->curtoken |= LJJ_TOKEN_INVALID;
        return NULL;
    }
    return root;
}

static jj_jsonobj* ljj_lexstate_parsearr(ljj_lexstate* state,  // NOLINT
                                         const char* name) {
    jj_jsonobj* root = jj_new_jsonarr(name);
    while (true) {
        ljj_lex_next(state);
        if (state->curtoken == ']') {
            break;
        }
        jj_jsonobj* obj = ljj_lexstate_parsenode(state, NULL, NULL);
        if (!obj) {
            jj_free(root);
            return NULL;
        }
        ojj_arrappend(root->data.arrval, obj);
        ljj_lex_skip_whitespace(state);
        ljj_lex_next(state);
        if (state->curtoken != ',') {
            break;
        }
    }
    if (state->curtoken != ']') {
        jj_free(root);
        if (!LJJ_LEXSTATE_ISINVALID(state))
            state->curtoken |= LJJ_TOKEN_INVALID;
        return NULL;
    }
    return root;
}

jj_jsonobj* jj_parse(const char* json_str, uint32_t length) {
    ljj_lexstate* state = ljj_new_lexstate(json_str, length);
    ljj_lex_next(state);
    jj_jsonobj* root = ljj_lexstate_parsenode(state, NULL, NULL);
    if (LJJ_LEXSTATE_ISINVALID(state)) {
        if (root) jj_free(root);
        LJJ_LEXSTATE_ERR(state);
    }
    ljj_lex_next(state);
    if (state->curtoken != LJJ_TOKEN_EOF) {
        if (root) jj_free(root);
        LJJ_LEXSTATE_ERR(state);
    }
    if (!root) {
        LJJ_LEXSTATE_ERR(state);
    }
    ljj_free_lexstate(state);
    return root;
}

void sjj_tostr_putindent(charvec* strbuf, int depth, jj_tostr_config* config) {
    if (config->formatted) {
        for (int i = 0; i < depth; i++) {
            for (int j = 0; j < config->indent; j++) {
                charvec_append(strbuf, ' ');
            }
        }
    }
}

void sjj_tostr_str(charvec* strbuf, char* data, int depth,
                   jj_tostr_config* config) {
    charvec_append(strbuf, '"');
    charvec_appendn(strbuf, data, strlen(data));
    charvec_append(strbuf, '"');
}

void sjj_tostr_jstr(charvec* strbuf, jj_jsondata data, int depth,
                    jj_tostr_config* config, bool inarr) {
    if (inarr) sjj_tostr_putindent(strbuf, depth, config);
    sjj_tostr_str(strbuf, data.strval, depth, config);
}

void sjj_tostr_jbool(charvec* strbuf, jj_jsondata data, int depth,
                     jj_tostr_config* config, bool inarr) {
    if (inarr) sjj_tostr_putindent(strbuf, depth, config);
    charvec_appendn(strbuf, data.boolval ? "true" : "false",
                    data.boolval ? 4 : 5);
}

void sjj_tostr_jint(charvec* strbuf, jj_jsondata data, int depth,
                    jj_tostr_config* config, bool inarr) {
    if (inarr) sjj_tostr_putindent(strbuf, depth, config);
    char buf[30];
    int len = sprintf(buf, "%lld", data.intval);
    charvec_appendn(strbuf, buf, len);
}

void sjj_tostr_jfloat(charvec* strbuf, jj_jsondata data, int depth,
                      jj_tostr_config* config, bool inarr) {
    if (inarr) sjj_tostr_putindent(strbuf, depth, config);
    char buf[30];
    int len = sprintf(buf, "%.17Lg", data.floatval);
    charvec_appendn(strbuf, buf, len);
}

void sjj_tostr_jnull(charvec* strbuf, int depth, jj_tostr_config* config,
                     bool inarr) {
    if (inarr) sjj_tostr_putindent(strbuf, depth, config);
    charvec_appendn(strbuf, "null", 4);
}

void sjj_tostr_jobj(charvec* strbuf, jj_jsondata data, int depth,
                    jj_tostr_config* config, bool inarr) {
    if (inarr) sjj_tostr_putindent(strbuf, depth, config);
    size_t cnt = hashmap_count(data.objval);
    if (cnt == 0) {
        charvec_appendn(strbuf, "{}", 2);
        return;
    }
    charvec_append(strbuf, '{');
    if (config->formatted) {
        charvec_append(strbuf, '\n');
    }
    size_t i = 0;
    jj_jsonobj* obj;
    size_t cur = 0;
    while (hashmap_iter(data.objval, &i, (void**)&obj)) {
        cur++;
        sjj_tostr(obj, strbuf, depth + 1, config, false);
        if (cur == cnt) {
            break;
        }
        charvec_append(strbuf, ',');
        if (config->formatted) {
            charvec_append(strbuf, '\n');
        } else if (config->sp) {
            charvec_append(strbuf, ' ');
        }
    }
    if (config->formatted) {
        charvec_append(strbuf, '\n');
        sjj_tostr_putindent(strbuf, depth, config);
    }
    charvec_append(strbuf, '}');
}

void sjj_tostr_jarr(charvec* strbuf, jj_jsondata data, int depth,
                    jj_tostr_config* config, bool inarr) {
    if (inarr) sjj_tostr_putindent(strbuf, depth, config);
    if (data.arrval->length == 0) {
        charvec_appendn(strbuf, "[]", 2);
        return;
    }
    charvec_append(strbuf, '[');
    if (config->formatted) {
        charvec_append(strbuf, '\n');
    }
    // TODO: if arr is short, put it in one line.
    jj_jsonobj* obj;
    size_t i = 0;
    while (true) {
        sjj_tostr(data.arrval->arr + i, strbuf, depth + 1, config, true);
        if (i == data.arrval->length - 1) {
            break;
        }
        charvec_append(strbuf, ',');
        if (config->formatted) {
            charvec_append(strbuf, '\n');
        } else if (config->sp) {
            charvec_append(strbuf, ' ');
        }
        i++;
    }
    if (config->formatted) {
        charvec_append(strbuf, '\n');
        sjj_tostr_putindent(strbuf, depth, config);
    }
    charvec_append(strbuf, ']');
}

int sjj_tostr_jdata(jj_jsonobj* obj, charvec* strbuf, int depth,
                    jj_tostr_config* config, bool inarr) {
    switch (obj->type) {
        case JJ_VALTYPE_BOOL:
            sjj_tostr_jbool(strbuf, obj->data, depth, config, inarr);
            break;
        case JJ_VALTYPE_STR:
            sjj_tostr_jstr(strbuf, obj->data, depth, config, inarr);
            break;
        case JJ_VALTYPE_INT:
            sjj_tostr_jint(strbuf, obj->data, depth, config, inarr);
            break;
        case JJ_VALTYPE_FLOAT:
            sjj_tostr_jfloat(strbuf, obj->data, depth, config, inarr);
            break;
        case JJ_VALTYPE_NULL:
            sjj_tostr_jnull(strbuf, depth, config, inarr);
            break;
        case JJ_VALTYPE_OBJ:
            sjj_tostr_jobj(strbuf, obj->data, depth, config, inarr);
            break;
        case JJ_VALTYPE_ARR:
            sjj_tostr_jarr(strbuf, obj->data, depth, config, inarr);
            break;
        default:
            return 1;
    }
    return 0;
}

int sjj_tostr(jj_jsonobj* obj, charvec* strbuf, int depth,
              jj_tostr_config* config, bool inarr) {
    if (obj->name != NULL) {
        sjj_tostr_putindent(strbuf, depth, config);
        sjj_tostr_str(strbuf, obj->name, depth, config);
        charvec_append(strbuf, ':');
        if (config->sp) {
            charvec_append(strbuf, ' ');
        }
    }
    return sjj_tostr_jdata(obj, strbuf, depth, config, inarr);
}

char* jj_tostr(jj_jsonobj* obj, jj_tostr_config* config) {
    charvec* strbuf = charvec_new(30);
    int ret = sjj_tostr(obj, strbuf, 0, config, false);
    if (ret != 0) {
        charvec_free(strbuf);
        return NULL;
    }
    char* str = charvec_tostr(strbuf);
    charvec_free(strbuf);
    return str;
}
