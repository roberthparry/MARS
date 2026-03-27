#ifndef STRING_INTERNAL_H
#define STRING_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "ustring.h"

struct string_t {
    char  *data;
    size_t len;
    size_t cap;
};

int string_reserve(string_t *s, size_t needed);
uint32_t utf8_decode(const char *s, size_t len, size_t *adv);

#endif

