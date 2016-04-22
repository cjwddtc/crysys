#include "database.h"
#include "encrypt.h"
#include "network.h"
#include "tran.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int port_l;
uint64_t key;
typedef struct {
  uint16_t count;
  uint16_t step;
  uint16_t buf_size;
  uint32_t file_size;
  FILE *fp;
} info;
void *OnCreate() {
  info *p = malloc(sizeof(info));
  memset(p, 0, sizeof(info));
  p->count = rand();
  set_key(key);
  return p;
}
void OnDestroy(void *ptr) { free(ptr); }
void OnMessage(char *in_ptr, info *data, size_t size) {
  switch (data->step) {
  case 0:
    set_key(*(uint64_t *)in_ptr);
    *(uint16_t *)write_data(2) = data->count++;
    data->step = 1;
    break;

  case 1: {
    file_send_info *ptr = (file_send_info *)in_ptr;
    if (ptr->seq == data->count++) {
      data->buf_size = ptr->size;
      data->fp = fopen(ptr->name, "rb");
      data->step = 2;
      file_recive_info *info =
          (file_recive_info *)write_data(sizeof(file_recive_info));
      info->seq = data->count++;
      info->size = get_file_size(ptr->name);
      data->file_size = info->size;
    } else
      data->count--;
    break;
  }
  case 2:
    if (*(uint16_t *)in_ptr == data->count++) {
      char *ptr;
      size_t size;
      if (data->file_size > data->buf_size - 2) {
        size = data->buf_size - 2;
        data->file_size -= size;
      } else {
        size = data->file_size;
        data->step = 3;
      }
      ptr = (char *)write_data(size + 2);
      *(uint16_t *)ptr = data->count++;
      fread(ptr + 2, 1, size, data->fp);
      break;
    } else
      data->count--;
    break;
  case 3:
    if (*(uint16_t *)in_ptr == data->count++) {
      fclose(data->fp);
      close_session();
    } else {
      data->count++;
    }
  default:
    break;
  }
}
typedef struct {
  uint16_t step;
  uint16_t count;
} c_info;

void *c_OnCreate() {
  c_info *p = (c_info *)malloc(sizeof(info));
  memset(p, 0, sizeof(c_info));
  FILE *fp = fopen("config.con", "rb");
  uint64_t key;
  char name[128];
  fread(&key, 8, 1, fp);
  size_t len = fread(name, 1, 128, fp);
  fwrite(name, 1, len, stdout);
  memcpy(write_data(len), name, len);
  set_key(key);
  fclose(fp);
  return p;
}

void c_OnDestroy(void *ptr) { free(ptr); }
void c_OnMessage(char *in_ptr, c_info *data, size_t size) {
  switch (data->step) {
  case 0:
    data->count = *(uint16_t *)in_ptr + 1;
    get_info *info = write_data(sizeof(get_info));
    info->seq = data->count++;
    info->size = port_l;
    info->op = REGISTER;
    close_session();
    break;
  }
}
int main() {
  FILE *fp = fopen("client.ini", "r");
  char ip[64];
  int port;
  int size;
  fscanf(fp, "server_ip=%s\n", ip);
  fscanf(fp, "server_port=%d\n", &port);
  fscanf(fp, "local_bufsize=%*d\n");
  fscanf(fp, "local_port=%d\n", &port_l);
  fscanf(fp, "server_bufsize=%d\n", &size);
  fclose(fp);
  fp = fopen("config.con", "rb");
  fread(&key, 8, 1, fp);
  fclose(fp);
  create_client(ip, port, size, c_OnCreate, (MessageHandle)c_OnMessage,
                c_OnDestroy, 0);
  create_server(port_l, size, OnCreate, (MessageHandle)OnMessage, OnDestroy, 0);
  start();
}
