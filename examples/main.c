#include "jj.h"

int main() {
    const char* j = "{\"abcdefg\": 3}";
    jj_jsonobj* jobj = jj_parse(j, strlen(j));
    if (!jobj) {
        fprintf(stderr, "Error\n");
        return 1;
    }
    jj_jsonobj* child = jj_oget(jobj, "abcdefg");
    printf("%d %d %s %lld\n", jobj->type, child->type, child->name,
           child->data.intval);
    jj_free(jobj);
    return 0;
}
