#include "database.h"
#include <assert.h>
#include <iostream>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <string>
class run_sql_fail_exception : public std::exception {
public:
  std::string str;
  run_sql_fail_exception(std::string str_) : str(str_) {}
};
class sql_st {
  sqlite3_stmt *a;
  void bind_(const char *str, int n) {
    char *ptr = (char *)malloc(strlen(str) + 1);
    strcpy(ptr, str);
    assert(sqlite3_bind_text(a, n, ptr, -1, free) == SQLITE_OK);
  }
  void bind_(const void *ptr, int size, int n) {
    void *nptr = malloc(size);
    memcpy(nptr, ptr, size);
    assert(sqlite3_bind_blob(a, n, nptr, size, free) == SQLITE_OK);
  }
  void bind_(int m, int n) { assert(sqlite3_bind_int(a, n, m) == SQLITE_OK); }
  void reset() {
    sqlite3_reset(a);
    sqlite3_clear_bindings(a);
  }

public:
  ~sql_st() { sqlite3_finalize(a); }
  sql_st(sqlite3 *db, std::string sql) {
    sqlite3_prepare_v2(db, sql.c_str(), -1, &a, 0);
  }
  template <size_t n = 1, class... ARG>
  void bind(const void *ptr, int size, ARG... arg) {
    if (n == 1) {
      reset();
    }
    bind_(ptr, size, n);
    bind<n + 1>(arg...);
  }
  template <size_t n = 1, class... ARG> void bind(const char *a, ARG... arg) {
    if (n == 1) {
      reset();
    }
    bind_(a, n);
    bind<n + 1>(arg...);
  }
  template <size_t n = 1, class... ARG> void bind(int a, ARG... arg) {
    if (n == 1) {
      reset();
    }
    bind_(a, n);
    bind<n + 1>(arg...);
  }
  template <size_t n> void bind() {}
  class proxy {
    sqlite3_value *value;

  public:
    proxy(sqlite3_value *value_) : value(value_) {}
    operator const char *() { return (const char *)sqlite3_value_text(value); }
    operator const void *() { return sqlite3_value_blob(value); }
    operator int() { return sqlite3_value_int(value); }
  };
  bool run() {
    while (true) {
      switch (sqlite3_step(a)) {
      case SQLITE_BUSY:
        break;
      case SQLITE_DONE:
        return false;
      case SQLITE_ROW:
        return true;
      default:
        throw(run_sql_fail_exception(
            std::string(sqlite3_errmsg(sqlite3_db_handle(a)))));
        break;
      }
    }
  }
  proxy operator[](int n) { return proxy(sqlite3_column_value(a, n)); }
};
sqlite3 *db = 0;
void init(const char *ptr) { sqlite3_open(ptr, &db); }
void const *get_key(const char *id) {
  static sql_st st(db, "select key from person where id=?1");
  st.bind(id);
  if (st.run()) {
    return st[0];
  } else {
    return nullptr;
  }
}
unsigned add_person(const char *id, const void *key, size_t size) {
  static sql_st st(db, "insert into person VALUES (?1, \'0.0.0.0\',0, ?2)");
  st.bind(id, key, size);
  try {
    st.run();
  } catch (run_sql_fail_exception &e) {
    std::cout << e.str << std::endl;
    return 0;
  }
  return 1;
}
unsigned get_person(const char *id, const void **key, const char **addr,
                    int *port) {
  static sql_st st(db, "select key,ip,port from person where id=?1");
  st.bind(id);
  if (st.run()) {
    *key = st[0];
    *addr = st[1];
    *port = st[2];
    return 1;
  } else {
    return 0;
  }
}
unsigned update_addr(const char *id, const char *addr, int port) {
  static sql_st st(db, "update person set ip=?1,port=?2 where id=?3");
  st.bind(addr, port, id);
  return st.run();
}
unsigned add(const char *id, char *name, void *hash, size_t size) {
  static sql_st st(db, "insert into file VALUES (?1, ?2, ?3)");
  st.bind(id, name, hash, size);
  try {
    return st.run();
  } catch (run_sql_fail_exception &e) {
    std::cerr << e.str << std::endl;
    return false;
  }
}
char *search(const char *name, size_t *size) {
  static sql_st st(db, "select name from file where name like ?1");
  st.bind(name);
  printf("name:%s\n", name);
  *size = 1024;
  char *ptr = (char *)malloc(*size + 1);
  size_t use = 0;
  while (st.run()) {
    printf("find\n");
    const char *str = st[0];
    size_t len = strlen(str) + 1;
    while (*size <= len + use) {
      *size <<= 1;
      ptr = (char *)realloc(ptr, *size + 1);
    }
    memcpy(ptr + use, str, len - 1);
    use += len;
    ptr[use++] = '\n';
  }
  ptr = (char *)realloc(ptr, use);
  *size = use;
  return ptr;
}
void freepp(char **ptr) {
  char **q = ptr;
  while (*ptr)
    free(*ptr++);
  free(q);
}
unsigned get(const char *name, const char **id, const void **hash) {
  static sql_st st(db, "select id,hash from file where name=?1");
  st.bind(name);
  if (st.run()) {
    *id = st[0];
    *hash = st[1];
    return 1;
  } else {
    return 0;
  }
}
