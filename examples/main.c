#include "jj.h"

int main() {
    const char* j =
        "{\"a\": \"1\", \n \"b\": 2,\n\t\"c\": true\t,\n\"d\": "
        "3.1415926e+3,\"h\": {\"h1\": \"this is h1\"}}";
    jj_jsonobj* jobj = jj_parse(j, strlen(j));
    if (!jobj) {
        fprintf(stderr, "Error\n");
        return 1;
    }
    jj_jsontype_str s = jj_ogetstrref(jobj, "a");
    printf("%s\n", s);
    jj_jsontype_int i;
    jj_ogetint(jobj, "b", &i);
    printf("%lld\n", i);
    jj_jsontype_bool b;
    jj_ogetbool(jobj, "c", &b);
    printf("%d\n", b);
    jj_jsontype_float f;
    jj_ogetfloat(jobj, "d", &f);
    printf("%Lf\n", f);

    jj_jsonobj* h = jj_oget(jobj, "h");
    jj_jsontype_str hs = jj_ogetstrref(h, "h1");
    printf("%s\n", hs);

    jj_free(jobj);
    return 0;
}
