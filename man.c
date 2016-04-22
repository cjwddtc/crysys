#include "database.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int a, char **b) {
  if (a != 2) {
    return 1;
  } else {
    uint64_t key = 0;
    srand(clock() ^ time(0));
    key = (uint16_t)rand();
    key <<= 16;
    key |= (uint16_t)rand();
    key <<= 16;
    key |= (uint16_t)rand();
    key <<= 16;
    key |= (uint16_t)rand();
    init("asd.db");
    FILE *fp = fopen("config.con", "wb");
    fwrite(&key, 8, 1, fp);
    fprintf(fp, "%s", b[1]);
    printf("key:%llx\n", key);
    if (add_person(b[1], &key, 8)) {
      printf("add:%s,%llx\n", b[1], *(uint64_t *)get_key(b[1]));
      printf("success\n");
    } else {
      printf("fail\n");
    }
    return 0;
  }
}
