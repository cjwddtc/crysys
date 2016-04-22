#include "encrypt.h"
#include "network.h"
#include "tran.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *oper;
uint16_t buf_size;
typedef struct {
  uint16_t step;
  uint16_t count;
  uint16_t buf_size;
  uint32_t file_size;
  char name[64]; // file name
  char hash[16]; // file's hash
  char *file_ptr;
  char *cp_ptr;
  FILE *fp;
} info;

void *OnCreate() {
  info *p = (info *)malloc(sizeof(info));
  memset(p, 0, sizeof(info));
  FILE *fp = fopen("config.con", "rb");
  uint64_t key;
  char name[128];
  fread(&key, 8, 1, fp);
  size_t len = fread(name, 1, 128, fp);
  fclose(fp);
  memcpy(write_data(len), name, len);
  set_key(key);
  return p;
}

void OnDestroy(void *ptr) { free(ptr); }
void OnMessage(char *in_ptr, info *data, size_t size) {
  switch (data->step) {
  case 0: {
    data->count = *(uint16_t *)in_ptr + 1;
    get_info *info;
    switch (*oper) {
    case 'u':
    case 'd':
    case 's':
      info = (get_info *)write_data(sizeof(get_info));
      info->seq = data->count++;
      strcpy(info->name, oper + 1);
      strcpy(data->name, oper + 1);
      break;
    }
    switch (*oper) {
    case 'u':
      data->step = 1;
      info->op = UPLOAD;
      info->size = get_file_size(data->name);
      data->file_size = info->size;
      data->fp = fopen(data->name, "rb");
      break;
    case 'd':
      data->step = 2;
      info->op = DOWNLOAD;
      break;
    case 's':
      data->step = 3;
      info->op = SEARCH;
      break;
    default:
      printf("wrong input\n");
      break;
    }
    break;
  }
  case 1: {
    upload_info *info = (upload_info *)in_ptr;
    if (info->seq == data->count++) {
      data->buf_size = info->size;
      printf("%d\n", data->buf_size);
      data->step = 4;
      goto send;
    } else
      data->count--;
    break;
  }
  case 4:
    if (*(uint16_t *)in_ptr == data->count++) {
      char *ptr;
      size_t size;
    send:
      printf("%d\n", data->file_size);
      if (data->file_size > data->buf_size - 2) {
        size = data->buf_size - 2;
        data->file_size -= size;
      } else {
        size = data->file_size;
        data->step = 5;
      }
      ptr = (char *)write_data(size + 2);
      *(uint16_t *)ptr = data->count++;
      fread(ptr + 2, 1, size, data->fp);
      break;
    } else
      data->count--;
    break;
  case 5:
    fclose(data->fp);
    close_session();
    break;
  case 2: {
    download_info *info = (download_info *)in_ptr;
    reconnect(info->ip, info->port);
    memcpy(write_data(24), info->other_key, 24);
    set_key(info->key);
    memcpy(data->hash, info->hash, 16);
    data->step = 6;
    break;
  }
  case 6: {
    data->count = *(uint16_t *)in_ptr + 1;
    data->step = 7;
    file_send_info *ptr = (file_send_info *)write_data(sizeof(file_send_info));
    strcpy(ptr->name, data->name);
    ptr->seq = data->count++;
    ptr->size = buf_size;
    break;
  }
  case 7: {
    file_recive_info *ptr = (file_recive_info *)in_ptr;
    if (ptr->seq == data->count++) {
      data->file_size = ptr->size;
      data->file_ptr = (char *)malloc(ptr->size);
      data->cp_ptr = data->file_ptr;
      *(uint16_t *)write_data(2) = data->count++;
      data->step = 8;
    } else
      data->count--;
    break;
  }
  case 8:
    printf("reading\n");
    if (*(uint16_t *)in_ptr == data->count++) {
      size -= 2;
      in_ptr += 2;
      printf("revice:%p,%p,%d,%d\n", data->cp_ptr, data->file_ptr, size,
             data->file_size);
      memcpy(data->cp_ptr, in_ptr, size);
      data->cp_ptr += size;
      printf("dell:%p,%p,%d,%d\n", data->cp_ptr, data->file_ptr, size,
             data->file_size);
      *(uint16_t *)write_data(2) = data->count++;
      if (data->cp_ptr - data->file_size == data->file_ptr) {
        char hash[16];
        md5(data->file_ptr, data->file_size, hash);
        uint64_t *p = (uint64_t *)hash;
        if (memcmp(hash, data->hash, 16) != 0)
          printf("hash wrong please retry\n");
        else {
          printf("writing\n");
          data->fp = fopen(data->name, "wb");
          fwrite(data->file_ptr, 1, data->file_size, data->fp);
          fclose(data->fp);
        }
        close_session();
      }
    } else
      data->count--;
    break;
  case 3: {
    uint16_t seq = *(uint16_t *)in_ptr;
    in_ptr += 2;
    if (seq == data->count++) {
      fwrite(in_ptr, size - 2, 1, stdout);
      close_session();
    } else {
      printf("seq wrong\n");
      data->count--;
    }
    break;
  }
  }
}

int main(int arg, char *args[]) {
  if (arg != 2) {
    return 1;
  }
  FILE *fp = fopen("client.ini", "r");
  oper = args[1];
  char ip[64];
  int port;
  fscanf(fp, "server_ip=%s\n", ip);
  fscanf(fp, "server_port=%d\n", &port);
  fscanf(fp, "local_bufsize=%d\n", &buf_size);
  fclose(fp);
  create_client(ip, port, buf_size, OnCreate, (MessageHandle)OnMessage,
                OnDestroy, 0);
  start();
}
