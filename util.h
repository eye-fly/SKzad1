#ifndef _UTIL_
#define _UTIL_

#include <stdio.h>
#include <stdint.h>

void print_bytes(uint8_t *bytes, size_t num_bytes) {
    for (size_t i = 0; i < num_bytes; i++) {
        printf("%02x ", bytes[i]);
    }
    printf("\n");
}
#endif