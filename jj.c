#include "jj.h"

void jj_free(jj_jsonobj* root) {
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

inline void _jj_arrfree(jj_jsonarrdata* arr) {
    jj_free(arr->arr);
    free(arr);
}

void _jj_lex_read_str(_jj_lexstate* state) { exit(1145141919810ULL); }

void _jj_lex_read_val(_jj_lexstate* state) {
    bool isfloat = false;
    char cur;
    while (true) {
        if (_jj_lexstate_isatend(state)) {
            break;
        }
        cur = state->original[state->cur_idx];
        _jj_lexstate_nextchar(state);
        if (_jj_is_char_oneof(cur, " \t\r\n")) {
            _jj_lexstate_nextchar(state);
            break;
        }
        if (cur < '0' || cur > 'z') {
            state->curtoken = _JJ_TOKEN_INVALID;
            return;
        }
        _jj_lexstate_resize_strbuf(state);
        state->strbuf[state->buflen] = cur;
        state->buflen++;
    }
    if (_jj_lexstate_bufequals(state->strbuf, "null", 4)) {
        state->curtoken = _JJ_TOKEN_NULL;
        return;
    }
    if (_jj_lexstate_bufequals(state->strbuf, "true", 4)) {
        state->curtoken = _JJ_TOKEN_TRUE;
        return;
    }
    if (_jj_lexstate_bufequals(state->strbuf, "false", 5)) {
        state->curtoken = _JJ_TOKEN_FALSE;
        return;
    }
    if (_jj_str_contains_s(state->strbuf, state->buflen, '.')) {
        state->curtoken = _JJ_TOKEN_FLOAT;
        return;
    }
    state->curtoken = _JJ_TOKEN_INT;
    return;
}

void _jj_lex_next(_jj_lexstate* state) {
    char cur;
    while (true) {
        if (_jj_lexstate_isatend(state)) {
            state->curtoken = _JJ_TOKEN_EOF;
            return;
        }
        cur = state->original[state->cur_idx];
        if (_jj_is_char_oneof(cur, " \r\t\n")) {
            _jj_lexstate_nextchar(state);
            continue;
        }
        break;
    }
    if (_jj_is_char_oneof(cur, "{}:,[]")) {
        state->curtoken = cur;
        _jj_lexstate_nextchar(state);
        return;
    }
    if (cur == '"') {
        _jj_lex_read_str(state);
        return;
    }
    _jj_lex_read_val(state);
}

bool _jj_lexstate_getint(_jj_lexstate* state, jj_jsontype_int* result) {
    char* buf = malloc(state->buflen + 1);
    strncpy(buf, state->strbuf, state->buflen);
    buf[state->buflen] = 0;
    char* pend;
    jj_jsontype_int res = strtoll(buf, &pend, 0);
    if (pend != buf + state->buflen) {
        return false;
    }
    *result = res;
    return true;
}

jj_jsonobj* jj_parse(const char* json_str, uint32_t length) {
    _jj_lexstate* state = _jj_new_lexstate(json_str, length);
    _jj_lex_next(state);
    jj_jsonobj* root = NULL;
    switch (state->curtoken) {
        case _JJ_TOKEN_TRUE:
            root = jj_new_jsonbool(NULL, JJ_JSON_TRUE);
            break;
        case _JJ_TOKEN_FALSE:
            root = jj_new_jsonbool(NULL, JJ_JSON_FALSE);
            break;
        case _JJ_TOKEN_INT:
            jj_jsontype_int res;
            if (!_jj_lexstate_getint(state, &res)) {
                _jj_lexstate_err(state);
                return NULL;
            }
            root = jj_new_jsonint(NULL, res);
        default:
            break;
    }
    _jj_lex_next(state);
    if (state->curtoken != _JJ_TOKEN_EOF) {
        _jj_lexstate_err(state);
        if (root) jj_free(root);
        return NULL;
    }
    return root;
}
