#include "jj.h"

void main() {
    const char* j = "114514";
    jj_jsonobj* jobj = jj_parse(j, strlen(j));
    printf("%d %lld\n", jobj->type, jobj->data.intval);
}
