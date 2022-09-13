#include "jj.h"

void jj_oput(jj_jsonobj* obj, jj_jsonobj* val) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return;
    }
    hashmap_set(obj->data.objval, val);
}

jj_jsonobj* jj_oget(jj_jsonobj* obj, const char* name) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return NULL;
    }
    jj_jsonobj* g =
        hashmap_get(obj->data.objval, &(jj_jsonobj){.name = (char*)name});
    return g;
}

void jj_free(jj_jsonobj* root) {  // NOLINT
    if (root->name) free(root->name);
    switch (root->type) {
        case JJ_VALTYPE_STR:
            free(root->data.strval);
            break;
        case JJ_VALTYPE_OBJ:
            ojj_hashmap_free(root->data.objval);
            break;
        case JJ_VALTYPE_ARR:
            ojj_arrfree(root->data.arrval);
            break;
        default:
            break;
    }
    free(root);
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
    if (state->buflen == 0) {
        state->curtoken = LJJ_TOKEN_INVALID;
        return;
    }
    if (state->strbuf[0] == '+') {
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
    bool isinvalid;
    if (ujj_str_isjsonint(state->strbuf, state->buflen, &isinvalid)) {
        if (isinvalid) {
            state->curtoken = LJJ_TOKEN_INVALID;
            return;
        }
        state->curtoken = LJJ_TOKEN_INT;
        return;
    }
    if (!ujj_str_isvalidjsonfloat(state->strbuf, state->buflen)) {
        state->curtoken = LJJ_TOKEN_INVALID;
        return;
    }
    state->curtoken = LJJ_TOKEN_FLOAT;
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
    return ujj_clonestr(state->strbuf, state->buflen);
}

static bool ljj_lexstate_getint(ljj_lexstate* state, jj_jsontype_int* result) {
    char* buf = ljj_lexstate_getstr(state);
    if (!buf) return false;
    char* pend;
    jj_jsontype_int res = strtoll(buf, &pend, 10);
    if (pend != buf + state->buflen) {
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
    if (pend != buf + state->buflen) {
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
            new = jj_new_jsonstr(name, s);
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
