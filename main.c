#include "database.h"
#include "encrypt.h"
#include "network.h"
#include "tran.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint16_t buf_size;
typedef struct {
  uint16_t count;
  uint16_t step;
  uint32_t size;
  void *file_ptr;
  char *cp_ptr;
  char name[64];
  char id[64];
} info;
void *OnCreate() {
  info *p = malloc(sizeof(info));
  memset(p, 0, sizeof(info));
  p->count = rand();
  return p;
}
void OnDestroy(void *ptr) { free(ptr); }
void OnMessage(char *in_ptr, info *data, size_t size) {
  switch (data->step) {
  case 0: {
    in_ptr[size] = 0;
    const void *key = get_key(in_ptr);
    if (key) {
      set_key(*(uint64_t *)key);
      strcpy(data->id, in_ptr);
      *(uint16_t *)write_data(2) = data->count;
      data->step = 1;
      data->count++;
    }
    break;
  }
  case 1: {
    get_info *ptr = (get_info *)in_ptr;
    if (data->count++ == ptr->seq) {
      switch (ptr->op) {
      case SEARCH: {
        size_t size;
        char *p = search(ptr->name, &size);
        uint16_t *ret = (uint16_t *)write_data(size + 2);
        *ret++ = data->count++;
        memcpy(ret, p, size);
        free(p);
        close_session();
        break;
      }
      case UPLOAD: {
        upload_info *info = (upload_info *)write_data(sizeof(upload_info));
        info->seq = data->count++;
        info->size = buf_size;
        data->step = 2;
        data->file_ptr = malloc(ptr->size);
        data->cp_ptr = (char *)data->file_ptr;
        data->size = ptr->size;
        strcpy(data->name, ptr->name);
        break;
      }
      case DOWNLOAD: {
        download_info *info =
            (download_info *)write_data(sizeof(download_info));
        info->seq = data->count++;
        const char *id;
        const void *hash;
        if (get(ptr->name, &id, &hash)) {
          memcpy(info->hash, hash, 16);

          info->key = (uint16_t)rand();
          info->key <<= 16;
          info->key |= (uint16_t)rand();
          info->key <<= 16;
          info->key |= (uint16_t)rand();
          info->key <<= 16;
          info->key |= (uint16_t)rand();

          const void *key_;
          const char *address;
          int port;
          get_person(id, &key_, &address, &port);
          uint64_t key = *(uint64_t *)key_;
          strcpy(info->ip, address);
          info->port = port;
          *(uint64_t *)info->other_key = info->key;
          size_t n = 8;
          signdes(info->other_key, &n, key);
          uint64_t *keys = (uint64_t *)info->other_key;
          assert(n == 24);
          close_session();
        } else {
          info->key = 0;
        }
        break;
      case REGISTER:
        printf("register,%s,%s,%d\n", data->id, get_address(), ptr->size);
        update_addr(data->id, get_address(), ptr->size);
        close_session();
        break;
      }
      }
    } else {
      data->count--;
    }
    break;
  }
  case 2: {
    if (*(uint16_t *)in_ptr == data->count++) {
      *(uint16_t *)write_data(2) = data->count++;
      in_ptr += 2;
      size -= 2;
      memcpy(data->cp_ptr, in_ptr, size);
      data->cp_ptr += size;
      if (data->cp_ptr - data->size == data->file_ptr) {
        char hash[16];
        md5(data->file_ptr, data->size, hash);
        add(data->id, data->name, hash, 16);
        free(data->file_ptr);
        data->file_ptr = 0;
        data->cp_ptr = 0;
        data->size = 0;
        data->step = 1;
        close_session();
      }
    } else {
      data->count--;
    }
    break;
  }
  }
}

int main() {
  FILE *fp = fopen("server.ini", "r");
  init("asd.db");
  int port;
  fscanf(fp, "port=%d\n", &port);
  fscanf(fp, "bufsize=%d\n", &buf_size);
  create_server(port, buf_size, OnCreate, (MessageHandle)OnMessage, OnDestroy,
                0);
  start();
}
