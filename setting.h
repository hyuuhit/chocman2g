#ifndef _SETTING_H_
#define _SETTING_H_

typedef struct setting_t{
    char *key;
    char *value;
    struct setting_t *next;
} setting_t;

setting_t *setting_create(const char *file);
void setting_destroy(setting_t *s);
const char *setting_get_str(setting_t *s, const char *key);
int setting_get_int(setting_t *s, const char *key, int default_value);

#endif
