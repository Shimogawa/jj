#include "jj.h"

int main() {
    const char* j = "\"abcdefg\"";
    jj_jsonobj* jobj = jj_parse(j, strlen(j));
    if (!jobj) {
        fprintf(stderr, "Error\n");
        return 1;
    }
    printf("%d %s\n", jobj->type, jobj->data.strval);
    jj_free(jobj);
    return 0;
}
