#include "jj.h"

bool jj_is_json_root(jj_jsonobj* json) { return json->name == NULL; }

bool jj_is_json_type(jj_jsonobj* json, jj_valtype type) {
    return json->type == type;
}

void jj_setname(jj_jsonobj* json, const char* name) {
    free(json->name);
    if (!name) {
        json->name = NULL;
        return;
    }
    size_t len = strlen(name);
    char* buf = malloc(len);
    strcpy(buf, name);
    json->name = buf;
}

jj_jsonobj* jj_new_empty_obj(const char* name) {
    jj_jsonobj* val = _JJ_MALLOC_NEW_JSONOBJ();
    _JJ_JSONOBJ_INIT_SETNAME(val, name);
    return val;
}

jj_jsonobj* jj_new_jsonbool(const char* name, jj_jsontype_bool b) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_BOOL;
    val->data.boolval = b;
    return val;
}

jj_jsonobj* jj_new_jsonint(const char* name, jj_jsontype_int i) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_INT;
    val->data.intval = i;
    return val;
}

jj_jsonobj* jj_new_jsonfloat(const char* name, jj_jsontype_float f) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_FLOAT;
    val->data.floatval = f;
    return val;
}

jj_jsonobj* jj_new_jsonstr(const char* name, jj_jsontype_str s) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_STR;
    val->data.strval = s;
    return val;
}

jj_jsonobj* jj_new_jsonnull(const char* name) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_NULL;
    return val;
}

jj_jsonobj* jj_new_jsonobj(const char* name) {
    jj_jsonobj* val = jj_new_empty_obj(name);
    val->type = JJ_VALTYPE_OBJ;
    val->data.objval = _jj_new_hashmap();
    return val;
}

void jj_oput(jj_jsonobj* obj, jj_jsonobj* val) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_OBJ)) {
        return;
    }
    hashmap_set(obj->data.objval, val);
}

jj_jsonobj* jj_aget(jj_jsonobj* obj, uint32_t idx) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_ARR)) {
        return NULL;
    }
    return obj->data.arrval->arr + idx;
}

bool jj_aappend(jj_jsonobj* obj, jj_jsonobj* val) {
    if (!jj_is_json_type(obj, JJ_VALTYPE_ARR)) {
        return false;
    }
    return _jj_arrappend(obj->data.arrval, val) == 0;
}

bool jj_ogetbool(jj_jsonobj* obj, const char* name, jj_jsontype_bool* result) {
    jj_jsonobj* ref = jj_oget(obj, name);
    if (!ref) {
        return false;
    }
    if (!jj_is_json_type(ref, JJ_VALTYPE_BOOL)) {
        return false;
    }
    *result = ref->data.boolval;
    return true;
}

void jj_free(jj_jsonobj* root) {  // NOLINT
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

void _jj_arrfree(jj_jsonarrdata* arr) {  // NOLINT
    jj_free(arr->arr);
    free(arr);
}

void _jj_lex_read_str(_jj_lexstate* state) {
    _jj_lexstate_nextchar(state);  // consume '"'
    char cur;
    while (true) {
        if (_jj_lexstate_isatend(state)) {
            state->curtoken = _JJ_TOKEN_INVALID;
            return;
        }
        cur = state->original[state->cur_idx];
        _jj_lexstate_nextchar(state);
        if (_jj_is_char_oneof(cur, "\r\n")) {
            state->curtoken = _JJ_TOKEN_INVALID;
            return;
        }
        if (cur == '"') {
            break;
        }
        // TODO: parse escapes
        _jj_lexstate_append_strbuf(state, cur);
    }
    state->curtoken = _JJ_TOKEN_STR;
}

void _jj_lex_read_val(_jj_lexstate* state) {
    _jj_lexstate_clear_strbuf(state);
    char cur;
    while (true) {
        if (_jj_lexstate_isatend(state)) {
            break;
        }
        cur = _JJ_LEXSTATE_CURCHAR(state);
        if (_jj_is_char_oneof(cur, " \t\r\n}],")) {
            break;
        }
        if ((cur < '0' || cur > 'z') && cur != '+' && cur != '-' &&
            cur != '.') {
            state->curtoken = _JJ_TOKEN_INVALID;
            return;
        }
        _jj_lexstate_append_strbuf(state, cur);
        _jj_lexstate_nextchar(state);
    }
    if (_jj_lexstate_bufequals(state, "null", 4)) {
        state->curtoken = _JJ_TOKEN_NULL;
        return;
    }
    if (_jj_lexstate_bufequals(state, "true", 4)) {
        state->curtoken = _JJ_TOKEN_TRUE;
        return;
    }
    if (_jj_lexstate_bufequals(state, "false", 5)) {
        state->curtoken = _JJ_TOKEN_FALSE;
        return;
    }
    bool isinvalid;
    if (_jj_str_isint(state->strbuf, state->buflen, &isinvalid)) {
        if (isinvalid) {
            state->curtoken = _JJ_TOKEN_INVALID;
            return;
        }
        state->curtoken = _JJ_TOKEN_INT;
        return;
    }
    state->curtoken = _JJ_TOKEN_FLOAT;
}

void _jj_lex_next(_jj_lexstate* state) {
    char cur;
    while (true) {
        if (_jj_lexstate_isatend(state)) {
            state->curtoken = _JJ_TOKEN_EOF;
            return;
        }
        cur = _JJ_LEXSTATE_CURCHAR(state);
        if (_jj_is_char_oneof(cur, " \r\t\n")) {
            _jj_lexstate_nextchar(state);
            continue;
        }
        break;
    }
    if (_jj_is_char_oneof(cur, "{}:,[]")) {
        state->curtoken = (uint8_t)cur;
        _jj_lexstate_nextchar(state);
        return;
    }
    if (cur == '"') {
        _jj_lex_read_str(state);
        return;
    }
    _jj_lex_read_val(state);
}

jj_jsontype_str _jj_lexstate_getstr(_jj_lexstate* state) {
    jj_jsontype_str s = malloc(state->buflen + 1);
    if (!s) return NULL;
    strncpy(s, state->strbuf, state->buflen);
    s[state->buflen] = 0;
    return s;
}

bool _jj_lexstate_getint(_jj_lexstate* state, jj_jsontype_int* result) {
    char* buf = _jj_lexstate_getstr(state);
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

bool _jj_lexstate_getfloat(_jj_lexstate* state, jj_jsontype_float* result) {
    char* buf = _jj_lexstate_getstr(state);
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

jj_jsonobj* _jj_lexstate_parsenode(_jj_lexstate* state, jj_jsonobj* root,
                                   const char* name) {
    jj_jsonobj* new;
    switch (state->curtoken) {
        case _JJ_TOKEN_TRUE:
            new = jj_new_jsonbool(name, JJ_JSON_TRUE);
            break;
        case _JJ_TOKEN_FALSE:
            new = jj_new_jsonbool(name, JJ_JSON_FALSE);
            break;
        case _JJ_TOKEN_INT: {
            jj_jsontype_int res;
            if (!_jj_lexstate_getint(state, &res)) {
                state->curtoken = _JJ_TOKEN_INVALID;
                return NULL;
            }
            new = jj_new_jsonint(name, res);
            break;
        }
        case _JJ_TOKEN_FLOAT: {
            jj_jsontype_float res;
            if (!_jj_lexstate_getfloat(state, &res)) {
                state->curtoken = _JJ_TOKEN_INVALID;
                return NULL;
            }
            new = jj_new_jsonfloat(name, res);
            break;
        }
        case _JJ_TOKEN_STR: {
            jj_jsontype_str s = _jj_lexstate_getstr(state);
            new = jj_new_jsonstr(name, s);
            break;
        }
        case '{': {
            new = _jj_lexstate_parseobj(state, name);
            break;
        }
        default:
            state->curtoken = _JJ_TOKEN_INVALID;
            return NULL;
    }
    if (!new) {
        state->curtoken = _JJ_TOKEN_INVALID;
        return NULL;
    }
    if (!name) {
        return new;
    }
    jj_oput(root, new);
    return root;
}

jj_jsonobj* _jj_lexstate_parseobj(_jj_lexstate* state, const char* name) {
    _jj_lex_next(state);
    if (state->curtoken != _JJ_TOKEN_STR) {
        state->curtoken = _JJ_TOKEN_INVALID;
        return NULL;
    }
    jj_jsonobj* root = jj_new_jsonobj(name);
    char* propname = NULL;
    while (true) {
        free(propname);
        propname = _jj_lexstate_getstr(state);
        if (!propname) {
            return NULL;
        }
        _jj_lex_skip_whitespace(state);
        if (_JJ_LEXSTATE_CURCHAR(state) != ':') {
            free(propname);
            free(root);
            if (state->curtoken != _JJ_TOKEN_EOF)
                state->curtoken = _JJ_TOKEN_INVALID;
            return NULL;
        }
        _jj_lexstate_nextchar(state);  // consume ':'
        _jj_lex_skip_whitespace(state);
        _jj_lex_next(state);
        _jj_lexstate_parsenode(state, root, propname);
        _jj_lex_skip_whitespace(state);
        _jj_lex_next(state);
        if (state->curtoken != ',') {
            break;
        }
        _jj_lexstate_nextchar(state);
    }
    free(propname);
    if (state->curtoken != '}') {
        free(root);
        if (state->curtoken != _JJ_TOKEN_EOF)
            state->curtoken = _JJ_TOKEN_INVALID;
        return NULL;
    }
    return root;
}

jj_jsonobj* jj_parse(const char* json_str, uint32_t length) {
    _jj_lexstate* state = _jj_new_lexstate(json_str, length);
    _jj_lex_next(state);
    jj_jsonobj* root = _jj_lexstate_parsenode(state, NULL, NULL);
    _jj_lex_next(state);
    if (state->curtoken != _JJ_TOKEN_EOF) {
        if (root) jj_free(root);
        _JJ_LEXSTATE_ERR(state);
    }
    if (!root) {
        _JJ_LEXSTATE_ERR(state);
    }
    _jj_free_lexstate(state);
    return root;
}
