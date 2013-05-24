#include "mem.h"
#include <stdlib.h>
#include <string.h>

void *mem_malloc(size_t size) {
    void *m = malloc(size);
    if(m == NULL) abort();
    return m;
}

void *mem_realloc(void *ptr, size_t size) {
    void *m = realloc(ptr, size);
    if(m == NULL) abort();
    return m;
}

void *mem_calloc(size_t nmemb, size_t size) {
    void *m = calloc(nmemb, size);
    if(m == NULL) abort();
    return m;
}

char *mem_strdup(const char *s) {
    char *m = strdup(s);
    if(m == NULL) abort();
    return m;
}
