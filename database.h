#ifndef LOGIC_H
#define LOGIC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void init(const char *ptr);
const void *get_key(const char *id);
unsigned add_person(const char *id, const void *key, size_t size);
unsigned get_person(const char *id, const void **key, const char **addr,
                    int *port);
unsigned update_addr(const char *id, const char *addr, int port);
unsigned add(const char *id, char *name, void *hash, size_t size);
char *search(const char *name, size_t *size);
unsigned get(const char *name, const char **id, const void **hash);
#ifdef __cplusplus
}
#endif
#endif
