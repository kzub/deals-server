#ifndef SRC_STATSD_CLIENT_HPP
#define SRC_STATSD_CLIENT_HPP

#include <iostream>
#include <map>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace statsd {

struct _ClientData {
  int sock;
  struct sockaddr_in server;

  std::string ns;
  std::string host;
  short port;
  bool init;

  char errmsg[1024];
};

class Tags : public std::map<const std::string, std::string> {
  using KeyValue = const std::pair<const std::string, const std::string>;

 public:
  Tags(const std::initializer_list<KeyValue>&& kv) {
    for (const auto& val : kv) {
      this->insert(val);
    }
  };
};

class Client {
 public:
  Client(const std::string& host = "127.0.0.1", int port = 8125, const std::string& ns = "");
  ~Client();

 public:
  // you can config at anytime; client will use new address (useful for Singleton)
  void config(const std::string& host, int port, const std::string& ns = "");
  const char* errmsg();
  int send_to_daemon(const std::string&);

 public:
  int inc(const std::string& key, const Tags& tags = {{}}, float sample_rate = 1.0);
  int dec(const std::string& key, const Tags& tags = {{}}, float sample_rate = 1.0);
  int count(const std::string& key, size_t value, const Tags& tags = {{}}, float sample_rate = 1.0);
  int gauge(const std::string& key, size_t value, const Tags& tags = {{}}, float sample_rate = 1.0);
  int timing(const std::string& key, size_t ms, const Tags& tags = {{}}, float sample_rate = 1.0);

 public:
  /**
   * (Low Level Api) manually send a message
   * which might be composed of several lines.
   */
  int send(const std::string& message);

  /* (Low Level Api) manually send a message
   * type = "c", "g" or "ms"
   */
  int send(std::string key, size_t value, const Tags& tags, const std::string& type,
           float sample_rate);

 protected:
  int init();
  void cleanup(std::string& key);

 protected:
  struct _ClientData d;

  bool exit_ = false;
};

static Client metric;
};  // end namespace

#endif