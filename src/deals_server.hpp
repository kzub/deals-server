#ifndef SRC_DEALS_SERVER_HPP
#define SRC_DEALS_SERVER_HPP

#include <functional>
#include "deals_cheapest.hpp"
#include "deals_cheapest_by_country.hpp"
#include "deals_cheapest_by_date.hpp"
#include "deals_database.hpp"
#include "http.hpp"
#include "tcp_server.hpp"
#include "top_destinations.hpp"

namespace deals_srv {
//------------------------------------------------------
// Connection Context
//------------------------------------------------------
class Context {
 public:
  int anyvalue;
  http::HttpParser http;
};

//------------------------------------------------------
// DealsServer derivered from TCPServer
//------------------------------------------------------
class DealsServer : public srv::TCPServer<Context> {
 public:
  DealsServer(const std::string host, const uint16_t port) : srv::TCPServer<Context>(host, port) {
  }
  void process();
  void quit();

 private:
  void on_connect(Connection& conn) final override;
  void on_data(Connection& conn) final override;

  void addDeal(Connection& conn);
  void getTop(Connection& conn);
  void getUniqueRoutes(Connection& conn);
  void getDestiantionsTop(Connection& conn);
  void terminateWithError(Connection& conn, types::Error& err);
  void writeTopResult(Connection& conn, const std::vector<deals::DealInfo>&& result);

  // in memory databases
  deals::DealsDatabase db;
  top::TopDstDatabase db_dst;

  bool quit_request = false;
};
}  // namespace deals_srv

#endif