#ifndef _MEM_H_
#define _MEM_H_

#include <stdlib.h>
#include <string.h>

#define mem_free    free

void *mem_malloc(size_t size);
void *mem_realloc(void *ptr, size_t size);
void *mem_calloc(size_t nmemb, size_t size);
char *mem_strdup(const char *s);

#endif
