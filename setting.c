#include "setting.h"
#include "mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

static char **space_splite_str(const char *str) {
    char **response = NULL;
    const char *s = str;
    int num = 0;
    int sec_len = 0;
    while(*s) {
        if(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
            if(sec_len) {
                sec_len = 0;
                num ++;
            }
        }
        else {
            sec_len ++;
        }
        s ++;
    }
    if(sec_len)
        num ++;

    if(num) {
        response = (char **)mem_malloc(sizeof(char *) * (num + 1));
        response[num] = NULL;
        num = 0;
        sec_len = 0;
        s = str;
        while(*s) {
            if(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
                if(sec_len) {
                    response[num] = (char *)mem_malloc(sec_len + 1);
                    response[num][sec_len] = 0;
                    memcpy(response[num], s - sec_len, sec_len);
                    sec_len = 0;
                    num ++;
                }
            }
            else {
                sec_len ++;
            }
            s ++;
        }
        if(sec_len) {
            response[num] = (char *)mem_malloc(sec_len + 1);
            response[num][sec_len] = 0;
            memcpy(response[num], s - sec_len, sec_len);
        }
    }

    return response;
}

static void splite_str_free(char **s) {
    if(s == NULL)
        return;
    char **t = s;
    while(*t) {
        mem_free(*t);
        t++;
    }
    mem_free(s);
}

static setting_t *parse_setting(char *buf) {
    char *start = buf;
    char *end;
    setting_t *setting;
    setting_t *head = NULL;

    do{
        end = strchr(start, '\n');
        if(end)
            *end = 0;
        char **s = space_splite_str(start);
        if(s != NULL && s[0] != NULL && s[1] != NULL && s[0][0] != '#') {
            if(!(setting = (setting_t *)mem_malloc(sizeof(*setting))) ||
                    !(setting->key = mem_strdup(s[0])) ||
                    !(setting->value = mem_strdup(s[1]))
              ){
                abort();
            }
            setting->next = head;
            head = setting;
        }
        splite_str_free(s);

        if(end != NULL)
            start = end + 1;
    }while(end != NULL);

    return head;
}

setting_t *setting_create(const char *file) {
    int fd;
    int len;
    char *f_buf;
    char *buf;
    struct stat st;
    setting_t *setting = NULL;

    fd = open(file, O_RDONLY);
    if(fd == -1) {
        fprintf(stderr, "open \"%s\": %s\n", file, strerror(errno));
        return NULL;
    }
    if(-1 == fstat(fd, &st)) {
        perror("fstat");
        close(fd);
        return NULL;
    }
    len = st.st_size;

    f_buf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if(f_buf == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }

    buf = (char *)mem_malloc(len + 1);
    memcpy(buf, f_buf, len);
    buf[len] = 0;

    setting = parse_setting(buf);

    close(fd);
    munmap(f_buf, len);
    mem_free(buf);

    return setting;
}

void setting_destroy(setting_t *s) {
    setting_t *t;
    while(s) {
        mem_free(s->key);
        mem_free(s->value);
        t = s;
        s = s->next;
        mem_free(t);
    }
}

const char *setting_get_str(setting_t *s, const char *key) {
    while(s) {
        if(!strcasecmp(key, s->key))
            return s->value;
        s = s->next;
    }
    return NULL;
}

int setting_get_int(setting_t *s, const char *key, int default_value) {
    int ret_code;
    const char *strval = setting_get_str(s, key);

    if(!strval)
        return default_value;

    if (isdigit (strval[0]) || (strval[0] == '-' && isdigit(strval[1])))
        return atoi(strval);

    ret_code = default_value;
    if (!strcasecmp (strval, "On"))
            ret_code = 1;
    else if (!strcasecmp (strval, "Off"))
            ret_code = 0;
    else if (!strcasecmp (strval, "Yes"))
            ret_code = 1;
    else if (!strcasecmp (strval, "No"))
            ret_code = 0;
    else if (!strcasecmp (strval, "True"))
            ret_code = 1;
    else if (!strcasecmp (strval, "False"))
            ret_code = 0;
    else if (!strcasecmp (strval, "Enable"))
            ret_code = 1;
    else if (!strcasecmp (strval, "Disable"))
            ret_code = 0;
    else if (!strcasecmp (strval, "Enabled"))
            ret_code = 1;
    else if (!strcasecmp (strval, "Disabled"))
            ret_code = 0;

    return ret_code;
}
