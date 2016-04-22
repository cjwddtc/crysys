#include "network.h"
#include "encrypt.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/variant.hpp>
#include <iostream>
#include <map>

using namespace boost::placeholders;
using boost::bind;
using boost::asio::buffer;
using boost::asio::ip::tcp;
using boost::function;
using boost::asio::ip::address;
using boost::asio::async_write;
using boost::asio::async_read;
using boost::filesystem::file_size;
using endpoint = boost::asio::ip::tcp::endpoint;
boost::asio::io_service io_service;

typedef function<void(std::size_t)> read_handle;
typedef function<void(const boost::system::error_code &, bool is_read)>
    error_handle;

class dec_signals {
  unsigned count;
  function<void()> func;

public:
  bool is_valid;
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
    if (!is_valid) {
      if (!count) {
        io_service.post(func);
        is_valid = true;
      } else {
        is_valid = true;
        this->func = func;
      }
    }
  }
};

class stream {
  dec_signals write_count;
  dec_signals read_count;
  size_t size;
  read_handle func;
  error_handle efunc;

public:
  void *ptr;
  uint64_t key;
  stream(tcp::socket &&a, size_t size_, read_handle func_, error_handle efunc_)
      : soc(std::move(a)), size(size_ + 16), ptr(size != 16 ? malloc(size) : 0),
        key(0), func(func_), efunc(efunc_) {}
  tcp::socket soc;

  void read() {
    if (!write_count.is_valid) {
      ++read_count;
      soc.async_read_some(
          buffer(ptr, size),
          [this](const boost::system::error_code &ec, std::size_t size) {
            --read_count;
            if (ec != 0)
              efunc(ec, true);
            if (signddes(ptr, &size, key))
              func(size);
            if (ec == 0)
              this->read();
          });
    }
  }

  void *alloc(size_t size) {
    ++write_count;
    return malloc(size + 16);
  }
  void write(void *ptr, size_t size) {
    signdes(ptr, &size, key);
    async_write(
        soc, buffer(ptr, size),
        [ptr, this](const boost::system::error_code &ec, std::size_t size) {
          if (ec != 0)
            efunc(ec, false);
          free(ptr);
          --write_count;
        });
  }
  operator tcp::socket *() { return &soc; }
  void close(std::function<void()> func) {
    write_count.add(bind(&tcp::socket::close, &soc));
    read_count.add(func);
  }
};

class global_info {
public:
  MessageHandle OnMessage;
  DestroyHandle OnDestroy;
  CreateHandle OnCreate;
  ErrorHandle OnError;
  size_t size;
  global_info(CreateHandle OnCreate_, MessageHandle OnMessage_,
              DestroyHandle OnDestroy_, ErrorHandle OnError_, size_t size_)
      : OnMessage(OnMessage_), OnDestroy(OnDestroy_), OnCreate(OnCreate_),
        OnError(OnError_), size(size_) {}
};

class session;
session *current_session = 0;
class session {
  global_info *info;
  void *data;

public:
  stream stm;
  session(global_info *info_)
      : info(info_), data(0),
        stm(tcp::socket(io_service), info->size, bind(&session::read, this, _1),
            bind(&session::error_handle, this, _1, _2)) {
    printf("create:%p\n", this);
  }
  void start() {
    boost::system::error_code ec;
    current_session = this;
    data = info->OnCreate();
    stm.read();
  }
  void error_handle(const boost::system::error_code &ec, bool is_read) {
    std::cerr << ec << ":" << is_read << std::endl;
    if (info->OnError)
      info->OnError(is_read, data);
  }
  void *add_write(size_t size) {
    void *ptr = 0;
    ptr = stm.alloc(size);
    io_service.post(bind(&stream::write, &stm, ptr, size));
    return ptr;
  }
  void read(std::size_t bytes_transferred) {
    if (bytes_transferred) {
      current_session = this;
      info->OnMessage(stm.ptr, data, bytes_transferred);
    }
  }
  tcp::socket *get_soc() { return stm; }
  void connect(const char *addr, unsigned port) {
    tcp::socket *soc = stm;
    soc->close();
    soc->connect(endpoint(address::from_string(std::string(addr)), port));
    stm.key = 0;
  }
  void close() {
    stm.close([this]() {
      info->OnDestroy(data);
      printf("real_close:%p\n", this);
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
         MessageHandle OnMessage, DestroyHandle OnDestroy, ErrorHandle OnError)
      : acceptor(io_service, tcp::endpoint(tcp::v4(), port)),
        global_info(OnCreate, OnMessage, OnDestroy, OnError, size) {}
};

class client : public global_info {

public:
  client(size_t size, CreateHandle OnCreate, MessageHandle OnMessage,
         DestroyHandle OnDestroy, ErrorHandle OnError)
      : global_info(OnCreate, OnMessage, OnDestroy, OnError, size) {}
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
                   MessageHandle OnMessage, DestroyHandle OnDestroy,
                   ErrorHandle OnError) {

  server *s = new server(port, size, OnCreate, OnMessage, OnDestroy, OnError);
  s->listen();
}

void create_client(const char *addr, unsigned int port, size_t size,
                   CreateHandle OnCreate, MessageHandle OnMessage,
                   DestroyHandle OnDestroy, ErrorHandle OnError) {
  client *c = new client(size, OnCreate, OnMessage, OnDestroy, OnError);
  c->connect(addr, port);
}

void reconnect(const char *addr, unsigned port) {
  current_session->connect(addr, port);
}

size_t get_file_size(const char *ptr) {
  try {
    return file_size(ptr);
  } catch (...) {
    return 0;
  }
}
const char *get_address() {
  static std::string str;
  str = current_session->get_soc()->remote_endpoint().address().to_string();
  return str.c_str();
}
uint16_t get_port() {
  return current_session->get_soc()->remote_endpoint().port();
}
void set_key(uint64_t key) {
  io_service.post([key]() { current_session->stm.key = key; });
}

void *write_data(size_t size) { return current_session->add_write(size); }

void close_session() {
  io_service.post(bind(&session::close, current_session));
}

void start() { io_service.run(); }
void stop() { io_service.stop(); }
