#ifndef SRC_DEALS_SERVER_HPP
#define SRC_DEALS_SERVER_HPP

#include "deals.hpp"
#include "http.hpp"
#include "tcp_server.hpp"
#include "top_destinations.hpp"

namespace deals_srv {
class Context {
 public:
  int test;
  http::HttpParser http;
};

class DealsServer : public srv::TCPServer<Context> {
 public:
  DealsServer(uint16_t port) : srv::TCPServer<Context>(port) {
  }

 private:
  void on_connect(Connection& conn);
  void on_data(Connection& conn);

  void addDeal(Connection& conn);
  void getTop(Connection& conn);
  void getDestiantionsTop(Connection& conn);

  deals::DealsDatabase db;
  top::TopDstDatabase db_dst;
};
}

#endif