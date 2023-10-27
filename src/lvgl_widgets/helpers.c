#include "lithiumx.h"
#include <string.h>

char *lv_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    void *new = lv_mem_alloc(len);

    if (new == NULL)
        return NULL;

    return (char *)lv_memcpy(new, s, len);
}
