#include "encrypt.h"
#include <stdio.h>
#include <string.h>
void signdes(void *ptr, size_t *size, uint64_t key) {
  if (*size && key) {
    md5(ptr, *size, ptr + *size);
    *size += 16;
    des(ptr, *size, key);
  }
}
int signddes(void *ptr, size_t *size, uint64_t key) {
  if (*size && key) {
    ddes(ptr, *size, key);
    *size -= 16;
    char buf[16];
    md5(ptr, *size, buf);
    return !memcmp(ptr + *size, buf, 16);
  }
  return 1;
}
