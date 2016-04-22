#ifndef API_H
#define API_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void des(void *ptr, size_t len, uint64_t key);
void ddes(void *ptr, size_t len, uint64_t key);
void md5(void *ptr, size_t size, void *result);
void signdes(void *ptr, size_t *size, uint64_t key);
int signddes(void *ptr, size_t *size, uint64_t key);
#ifdef __cplusplus
}
#endif
#endif
