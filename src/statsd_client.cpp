#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "statsd_client.hpp"

namespace statsd {

inline bool fequal(float a, float b) {
  const float epsilon = 0.0001;
  return (fabs(a - b) < epsilon);
}

inline bool should_send(float sample_rate) {
  if (fequal(sample_rate, 1.0)) {
    return true;
  }

  float p = ((float)random() / RAND_MAX);
  return sample_rate > p;
}

Client::Client(const std::string& host, int port, const std::string& ns) {
  d.sock = -1;
  config(host, port, ns);
  srandom(time(NULL));
}

Client::~Client() {
  if (d.sock >= 0) {
    close(d.sock);
    d.sock = -1;
  }
}

void Client::config(const std::string& host, int port, const std::string& ns) {
  d.ns = ns;
  d.host = host;
  d.port = port;
  d.init = false;
  if (d.sock >= 0) {
    close(d.sock);
  }
  d.sock = -1;
}

int Client::init() {
  if (d.init) {
    return 0;
  }

  d.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (d.sock == -1) {
    std::cerr << "could not create socket, err=" << std::to_string(errno) << std::endl;
    return -1;
  }

  memset(&d.server, 0, sizeof(d.server));
  d.server.sin_family = AF_INET;
  d.server.sin_port = htons(d.port);

  int ret = inet_aton(d.host.c_str(), &d.server.sin_addr);
  if (ret == 0) {
    // host must be a domain, get it from internet
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo(d.host.c_str(), NULL, &hints, &result);
    if (ret) {
      close(d.sock);
      d.sock = -1;
      std::cerr << "getaddrinfo fail, error=" << std::to_string(ret)
                << ", msg=" << gai_strerror(ret) << std::endl;
      return -2;
    }
    struct sockaddr_in* host_addr = (struct sockaddr_in*)result->ai_addr;
    memcpy(&d.server.sin_addr, &host_addr->sin_addr, sizeof(struct in_addr));
    freeaddrinfo(result);
  }

  d.init = true;
  return 0;
}

/* will change the original std::string */
void Client::cleanup(std::string& key) {
  size_t pos = key.find_first_of(":|@");
  while (pos != std::string::npos) {
    key[pos] = '_';
    pos = key.find_first_of(":|@");
  }
}

int Client::dec(const std::string& key, const Tags& tags, float sample_rate) {
  return count(key, -1, tags, sample_rate);
}

int Client::inc(const std::string& key, const Tags& tags, float sample_rate) {
  return count(key, 1, tags, sample_rate);
}

int Client::count(const std::string& key, size_t value, const Tags& tags, float sample_rate) {
  return send(key, value, tags, "c", sample_rate);
}

int Client::gauge(const std::string& key, size_t value, const Tags& tags, float sample_rate) {
  return send(key, value, tags, "g", sample_rate);
}

int Client::timing(const std::string& key, size_t ms, const Tags& tags, float sample_rate) {
  return send(key, ms, tags, "ms", sample_rate);
}

int Client::send(std::string key, size_t value, const Tags& tags, const std::string& type,
                 float sample_rate) {
  if (!should_send(sample_rate)) {
    return 0;
  }
  cleanup(key);

  std::string tagstext = "";
  if (tags.size() > 0) {
    for (auto& t : tags) {
      tagstext = tagstext + "," + t.first + "=" + t.second;
    }
  }

  std::string buf = d.ns + key + tagstext + ":" + std::to_string(value) + "|" + type;

  if (!fequal(sample_rate, 1.0)) {
    buf = buf + "|@" + std::to_string(sample_rate);
  }

  return send(buf);
}

int Client::send(const std::string& message) {
  return send_to_daemon(message);
}

int Client::send_to_daemon(const std::string& message) {
  // std::cout << "send_to_daemon: " << message << std::endl;
  int ret = init();
  if (ret) {
    return ret;
  }
  ret = sendto(d.sock, message.data(), message.size(), 0, (struct sockaddr*)&d.server,
               sizeof(d.server));
  if (ret == -1) {
    std::cerr << "sendto server fail, host=" << d.host.c_str() << ":" << std::to_string(d.port)
              << ", err=" << std::to_string(errno) << std::endl;
    return -1;
  }

  return 0;
}

}  // namespace statsd