#include "jj.h"

int main() {
    const char* j =
        "{\"a\": \"1\", \n \"b\": 2,\n\t\"c\": true\t,\n\"d\": "
        "3.1415926e+3,\"h\": {\"h1\": \"this is h1\"},\n \"i\": [true, "
        "114.514, 114514, \"hi\", {\"i1\": 3, \"i2\": [\"nest\"]}, null]}";
    jj_jsonobj* jobj = jj_parse(j, strlen(j));
    if (!jobj) {
        fprintf(stderr, "Error\n");
        return 1;
    }
    jj_jsontype_str s = jj_ogetstrref(jobj, "a");
    printf("a: %s\n", s);
    jj_jsontype_int i;
    jj_ogetint(jobj, "b", &i);
    printf("b: %ld\n", i);
    jj_jsontype_bool b;
    jj_ogetbool(jobj, "c", &b);
    printf("c: %d\n", b);
    jj_jsontype_float f;
    jj_ogetfloat(jobj, "d", &f);
    printf("d: %Lf\n", f);

    jj_jsonobj* h = jj_oget(jobj, "h");
    s = jj_ogetstrref(h, "h1");
    printf("h.h1: %s\n", s);

    jj_jsonobj* iarr = jj_oget(jobj, "i");
    jj_agetbool(iarr, 0, &b);
    printf("i@0: %d\n", b);
    jj_agetfloat(iarr, 1, &f);
    printf("i@1: %Lf\n", f);
    jj_agetint(iarr, 2, &i);
    printf("i@2: %ld\n", i);
    s = jj_agetstrref(iarr, 3);
    printf("i@3: %s\n", s);

    jj_jsonobj* iobj = jj_aget(iarr, 4);
    jj_ogetint(iobj, "i1", &i);
    printf("i@4.i1: %ld\n", i);
    s = jj_agetstrref(jj_oget(iobj, "i2"), 0);
    printf("i@4.i2@0: %s\n", s);

    jj_free(jobj);

    j = "{\"a\": 3, \"b\": \"str\", \"c\": 3e10, \"d\": {\"da\": \"str2\", "
        "\"db\": 3.1415}, \"e\": [1, 2.5e3, null, true, \"hi\\u3065\", { }, "
        "[]]}";
    jobj = jj_parse(j, strlen(j));
    jj_tostr_config cfg = {
        .formatted = true, .indent = 2, .sp = true, .esc_unic = false};
    s = jj_tostr(jobj, &cfg);
    printf("%s\n", s);
    free(s);
    jj_free(jobj);

    j = "\"abc\\u4f60\\u597d吗\\r\\t\\n\\\\\"";
    jobj = jj_parse(j, strlen(j));
    s = jj_tostr(jobj, &cfg);
    printf("%s\n", s);
    free(s);
    cfg.esc_unic = true;
    s = jj_tostr(jobj, &cfg);
    printf("%s\n", s);
    free(s);
    jj_free(jobj);
    return 0;
}
