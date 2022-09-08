#include "jj.h"

void __jj_lex_read_str(__jj_lexstate* state) { exit(1145141919810ULL); }

void __jj_lex_read_val(__jj_lexstate* state) {
    bool isfloat = false;
    char cur;
    while (true) {
        cur = state->original[state->cur_idx];
        __jj_lexstate_nextchar(state);
        if (cur < '0' || cur > 'z') {
            state->curtoken = __JJ_TOKEN_INVALID;
            return;
        }
        __jj_lexstate_resize_strbuf(state);
        state->strbuf[state->buflen] = cur;
        state->buflen++;
    }
    if (strncmp(state->strbuf, "null", state->buflen)) {
        state->curtoken = __JJ_TOKEN_NULL;
        return;
    }
    if (__jj_str_contains_s(state->strbuf, state->buflen, '.')) {
        state->curtoken = __JJ_TOKEN_FLOAT;
        return;
    }
    state->curtoken = __JJ_TOKEN_INT;
    return;
}

void __jj_lex_next(__jj_lexstate* state) {
    if (state->cur_idx >= state->length) {
        return;
    }
    char cur;
    while (true) {
        cur = state->original[state->cur_idx];
        if (__jj_is_char_oneof(cur, " \r\t")) {
            state->col++;
            continue;
        }
        if (cur == '\n') {
            state->line++;
            continue;
        }
        break;
    }
    if (__jj_is_char_oneof(cur, "{}:,[]")) {
        __jj_lexstate_nextchar(state);
        state->curtoken = cur;
        return;
    }
    if (cur == '"') {
        __jj_lex_read_str(state);
        return;
    }
    __jj_lex_read_val(state);
}

jj_jsonobj* jj_parse(const char* json_str) {
    __jj_lexstate* state = __jj_new_lexstate();
    __jj_lex_next(state);
    if (!__jj_is_char_oneof(state->curtoken, "{[")) {
        __jj_lexstate_err(state);
        return NULL;
    }
    // todo
}
