#include "encrypt.h"
#include "network.h"
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/functional.hpp>
#include <boost/thread.hpp>
#include <boost/variant.hpp>
#include <iostream>
#include <map>
#include <windows.h>

using namespace boost::placeholders;
using boost::bind;
using boost::asio::buffer;
using boost::asio::ip::tcp;
using boost::asio::windows::stream_handle;
using boost::function;
using boost::asio::ip::address;
using boost::asio::async_write;
using boost::asio::async_read;
using boost::filesystem::file_size;
using boost::thread_specific_ptr;
using endpoint = boost::asio::ip::tcp::endpoint;
boost::asio::io_service io_service;

typedef function<void(std::size_t)> read_handle;
typedef function<void(const boost::system::error_code &, bool is_read)>
    error_handle;
class close_stream : public boost::static_visitor<> {
public:
  void operator()(stream_handle &a) {
    a.cancel();
    a.close();
    CloseHandle(a.native_handle());
  }
  void operator()(tcp::socket &a) {
    a.cancel();
    a.close();
  }
};

class dec_signals {
  unsigned count;
  function<void()> func;
  bool is_valid;

public:
  dec_signals() : count(0), is_valid(false) {}
  dec_signals(dec_signals &&other)
      : count(other.count), func(std::move(other.func)),
        is_valid(other.is_valid) {}
  dec_signals &operator++() {
    ++count;
    return *this;
  }
  dec_signals &operator--() {
    if (!--count && is_valid) {
      io_service.post(func);
    }
    return *this;
  }
  void add(std::function<void()> func) {
    if (!is_valid)
      if (!count) {
        io_service.post(func);
        is_valid = true;
      } else {
        is_valid = true;
        this->func = func;
      }
  }
};

class stream {
  dec_signals count;
  size_t size;
  unsigned flag;
  read_handle func;
  error_handle efunc;

public:
  void *ptr;
  uint64_t key;
  template <class T>
  stream(T &&a, size_t size_, unsigned flag_, read_handle func_,
         error_handle efunc_)
      : value(std::move(a)), size(size_), ptr(size ? malloc(size) : 0),
        flag(flag_), key(0), func(func_), efunc(efunc_) {}
  stream(stream &&stm)
      : value(std::move(stm.value)), size(stm.size), ptr(stm.ptr),
        count(std::move(stm.count)), flag(stm.flag), key(stm.key),
        func(stm.func), efunc(stm.efunc) {
    stm.ptr = 0;
    stm.size = 0;
    stm.flag = 0;
    stm.key = 0;
  }
  boost::variant<stream_handle, tcp::socket> value;
  void read() {
    if (flag & 0x1) {
      boost::apply_visitor(
          [this, func](auto &a) {
            a.async_read_some(buffer(ptr, size),
                              [this, func](const boost::system::error_code &ec,
                                           std::size_t size) {
                                if (ec != 0)
                                  efunc(ec, true);
                                else
                                  read();
                                if (signddes(ptr, &size, key))
                                  func(size);
                              });
          },
          value);
    }
  }
  void *alloc(size_t size) {
    ++count;
    return malloc(size + 16);
  }
  void write(void *ptr, size_t size, error_handle efunc) {
    if (flag & 0x2) {
      signdes(ptr, &size, key);
      boost::apply_visitor(
          [ptr, size, this](auto &a) {
            async_write(a, buffer(ptr, size),
                        [ptr, this](const boost::system::error_code &ec,
                                    std::size_t size) {
                          if (ec != 0)
                            efunc(ec, false);
                          free(ptr);
                          --count;
                        });
          },
          value);
    }
  }
  operator tcp::socket *() { return boost::get<tcp::socket>(&value); }
  void close(std::function<void()> func) {
    count.add([func, this]() {
      boost::apply_visitor(cl(), value);
      func();
    });
  }
};

class lsy_map {
  std::map<unsigned, stream> real_map;

public:
  void add(stream &&a, unsigned id) {
    real_map.insert(std::make_pair(id, std::move(a)));
  }

  stream &operator[](size_t id) {
    auto it = real_map.find(id);
    assert(it != real_map.end());
    return it->second;
  }

  bool contain(unsigned id) { return real_map.find(id) != real_map.end(); }
  void erase(unsigned id) {
    auto it = real_map.find(id);
    assert(it != real_map.end());
    it->second.close([this, id]() { real_map.erase(id); });
  }
  void clear(std::function<void()> func) {
    int *pi;
    if (!real_map.empty())
      pi = new int(real_map.size());
    for (auto &a : real_map) {
      a.second.close([pi, func, this]() {
        if (!--*pi) {
          delete pi;
          real_map.clear();
          io_service.post(func);
        }
      });
    }
  }
};

class global_info {
public:
  MessageHandle OnMessage;
  DestroyHandle OnDestroy;
  CreateHandle OnCreate;
  size_t size;
  global_info(CreateHandle OnCreate_, MessageHandle OnMessage_,
              DestroyHandle OnDestroy_, size_t size_)
      : OnMessage(OnMessage_), OnDestroy(OnDestroy_), OnCreate(OnCreate_),
        size(size_) {}
};

class session;
session *current_session;
class session {
  global_info *info;
  void *data;
  unsigned id;
  bool is_valid;

public:
  lsy_map map;
  session(global_info *info_) : info(info_), data(0), id(1), is_valid(true) {
    add(stream(tcp::socket(io_service), info->size, 0x3), 0);
  }
  void start() {
    boost::system::error_code ec;
    current_session = this;
    data = info->OnCreate();
    for (unsigned i = 0; i < id; i++)
      map[i].read(bind(&session::read, this, _1, _2, i));
  }
  void error_handle(const boost::system::error_code &ec, unsigned id,
                    bool is_read) {}
  void *add_write(size_t size, unsigned id) {
    void *ptr = 0;
    if (map.contain(id)) {
      ptr = map[i].alloc(size);
      io_service.post([ptr, size, id]() {
        assert(map.contain(id));
        map[i].write(ptr, size);
      });
    }
    return ptr;
  }
  void read(std::size_t bytes_transferred, unsigned id) {
    if (bytes_transferred && map.contain(id)) {
      current_session = this;
      info->OnMessage(map[id].ptr, data, bytes_transferred, id);
      write();
    }
    printf("fin\n");
  }
  tcp::socket *get_soc() { return map[0]; }
  template <class T> void add(T &&a, unsigned id, unsigned flag, size_t size) {
    map.add(stream(std::move(a), size, flag, bind(&session::read, this, _1, id),
                   bind(&session::error_handle, this, _1, id, _2)),
            id);
  }
  unsigned add_file(const char *ptr, unsigned flag, size_t size) {
    unsigned f = 0;
    unsigned ff = OPEN_ALWAYS;
    if (flag & 0x1) {
      f |= GENERIC_READ;
      ff = OPEN_EXISTING;
    }
    if (flag & 0x2) {
      f |= GENERIC_WRITE;
    };

    add(stream_handle(io_service,
                      CreateFile(ptr, f,
                                 f & GENERIC_WRITE ? 0 : FILE_SHARE_READ, 0, ff,
                                 FILE_FLAG_OVERLAPPED, 0)),

        id, flag, size);
    return id++;
  }
  unsigned add_file(const char *ptr, unsigned flag) {
    std::cout << file_size(ptr) << std::endl;
    return add_file(ptr, flag, flag & 0x1 ? file_size(ptr) : 0);
  }
  unsigned add_consle(unsigned flag) {
    return add_file("CONIN$", flag, flag & 0x1 ? info->size : 0);
  }
  void close() {
    is_valid = false;
    map.clear([this]() {
      info->OnDestroy(data);
      delete this;
    });
  }
};

class server : public global_info {
  tcp::acceptor acceptor;
  void create(const boost::system::error_code &ec, session *a) {
    if (ec != 0) {
      return;
    }
    listen();
    a->start();
  }

public:
  void listen() {
    session *a = new session(this);
    acceptor.async_accept(*a->get_soc(), bind(&server::create, this, _1, a));
  }
  server(unsigned int port, size_t size, CreateHandle OnCreate,
         MessageHandle OnMessage, DestroyHandle OnDestroy)
      : acceptor(io_service, tcp::endpoint(tcp::v4(), port)),
        global_info(OnCreate, OnMessage, OnDestroy, size) {}
};

class client : public global_info {

public:
  client(size_t size, CreateHandle OnCreate, MessageHandle OnMessage,
         DestroyHandle OnDestroy)
      : global_info(OnCreate, OnMessage, OnDestroy, size) {}
  void create(const boost::system::error_code &ec, session *a) {
    if (ec != 0) {
      return;
    }
    a->start();
  }
  void connect(const char *addr, unsigned long port) {
    session *a = new session(this);
    a->get_soc()->async_connect(
        endpoint(address::from_string(std::string(addr)), port),
        bind(&client::create, this, _1, a));
  }
};

void create_server(unsigned int port, size_t size, CreateHandle OnCreate,
                   MessageHandle OnMessage, DestroyHandle OnDestroy) {

  server *s = new server(port, size, OnCreate, OnMessage, OnDestroy);
  s->listen();
}

void create_client(const char *addr, unsigned int port, size_t size,
                   CreateHandle OnCreate, MessageHandle OnMessage,
                   DestroyHandle OnDestroy) {
  client *c = new client(size, OnCreate, OnMessage, OnDestroy);
  c->connect(addr, port);
}

unsigned open_file(char *filename, unsigned flag) {
  return current_session->add_file(filename, flag);
}

unsigned open_consle(unsigned flag) {
  return current_session->add_consle(flag);
}
size_t get_file_size(const char *ptr) {
  try {
    return file_size(ptr);
  } catch (...) {
    return 0;
  }
}
const char *get_address() {
#warning this function is not support the multithread
  static std::string str;
  str = current_session->get_soc()->remote_endpoint().address().to_string();
  return str.c_str();
}
uint16_t get_port() {
  return current_session->get_soc()->remote_endpoint().port();
}
void set_key(unsigned id, uint64_t key) {
  io_service.post([key, id]() {
    if (current_session->map.contain(id))
      current_session->map[id].key = key;
  });
}

void *write_data(unsigned id, size_t size) {
  return current_session->add_write(size, id);
}

void close_id(unsigned id) { return current_session->map.erase(id); }

void close_session() {
  io_service.post(bind(&session::close, current_session));
}
void start() { io_service.run(); }
void stop() { io_service.stop(); }
