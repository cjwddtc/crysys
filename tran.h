#ifndef TRAN_H
#define TRAN_H
#include <stdint.h>
enum server_type { SEARCH, DOWNLOAD, UPLOAD, REGISTER };
typedef struct {
  uint16_t seq;
  enum server_type op;
  char name[64];
  uint32_t size;
} get_info;

typedef struct {
  uint16_t seq;
  uint16_t size;
} upload_info;
typedef struct {
  uint16_t seq;
  uint16_t port;
  uint64_t key;
  char other_key[24];
  char ip[64];
  char hash[16];
} download_info;
typedef struct {
  uint16_t seq;
  uint16_t size;
  char name[64];
} file_send_info;
typedef struct {
  uint16_t seq;
  uint32_t size
} file_recive_info;

#endif
