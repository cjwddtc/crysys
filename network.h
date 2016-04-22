#ifndef NETWORK_H
#define NETWORK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*MessageHandle)(void *in_ptr, void *data, size_t ext);
typedef void *(*CreateHandle)();
typedef void (*ErrorHandle)(unsigned is_read, void *data);
typedef void (*DestroyHandle)(void *ptr);
typedef void (*ConnectHandle)(void *data);
void create_server(unsigned int port, size_t size, CreateHandle OnCreate,
                   MessageHandle OnMessage, DestroyHandle OnDestroy,
                   ErrorHandle OnError);
void create_client(const char *addr, unsigned int port, size_t size,
                   CreateHandle OnCreate, MessageHandle OnMessage,
                   DestroyHandle OnDestroy, ErrorHandle OnError);
void reconnect(const char *addr, unsigned port);

const char *get_address();
uint16_t get_port();
void set_key(uint64_t key);
void *write_data(size_t size);
void close_session();
size_t get_file_size(const char *);

void start();
void stop();

#ifdef __cplusplus
}
#endif
#endif
