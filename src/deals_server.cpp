#include "deals_server.hpp"
#include <fstream>

#include "deals.hpp"
#include "http.hpp"
#include "locks.hpp"
#include "timing.hpp"

namespace deals_srv {

/* --------------------------------------------------------
* DealsServer on connect
*----------------------------------------------------------*/
void DealsServer::on_connect(Connection& conn) {
  // std::cout << "new conn:" << conn.get_client_address() << std::endl;
}

/* --------------------------------------------------------
* DealsServer on data
*----------------------------------------------------------*/
void DealsServer::on_data(Connection& conn) {
  conn.context.http.write(conn.get_data());

  if (!conn.context.http.is_request_complete()) {
    return;
  }

  try {
    if (conn.context.http.request.method == "GET") {
      if (conn.context.http.request.query.path == "/deals/top") {
        getTop(conn);
        return;
      }

      if (conn.context.http.request.query.path == "/deals/truncate") {
        db.truncate();
        http::HttpResponse response(200, "OK", "cleaned\n");
        conn.close(response);
        return;
      }
    }

    else if (conn.context.http.request.method == "POST") {
      if (conn.context.http.request.query.path == "/deals/add") {
        addDeal(conn);
        return;
      }
    }

    // default response:
    conn.close(http::HttpResponse(404, "Not Found", "Method unknown\n"));

  } catch (...) {
    std::cout << "Request processing error" << std::endl;
    conn.close(http::HttpResponse(500, "Internal Server Error",
                                  "Request broke something inside me...\n"));
    return;
  }
}

/*---------------------------------------------------------
* DealsServer getTop
*-----------------------------------------------------------*/
void DealsServer::getTop(Connection& conn) {
  //-------------------------
  // origin/destinations
  std::string origin =
      utils::toUpperCase(conn.context.http.request.query.params["origin"]);
  std::string destinations = utils::toUpperCase(
      conn.context.http.request.query.params["destinations"]);

  if (origin.length() != 3) {
    conn.close(http::HttpResponse(400, "Bad origin", "Bad origin"));
    return;
  }

  //-------------------------
  // direct and flights with stops
  bool direct_flights = false;
  bool stops_flights = false;
  std::string direct =
      utils::toLowerCase(conn.context.http.request.query.params["direct_flights"]);

  if (direct == "") {
    direct_flights = true;
    stops_flights = true;
  } else if (direct == "true") {
    direct_flights = true;
  } else if (direct == "false") {
    stops_flights = true;
  } else {
    conn.close(http::HttpResponse(400, "Bad direct_flights parameter",
                                  "Bad direct_flights parameter"));
    return;
  }

  //-------------------------
  // search timelimit
  uint32_t max_lifetime_sec = 0;
  std::string timelimit = conn.context.http.request.query.params["timelimit"];

  if (timelimit.length() > 0) {
    try {
      max_lifetime_sec =
          std::stol(conn.context.http.request.query.params["timelimit"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad timelimit", "Bad timelimit"));
      return;
    }
  }

  //-------------------------
  // search limit
  uint16_t limit = 0;
  try {
    limit = std::stol(conn.context.http.request.query.params["deals_limit"]);
  } catch (...) {
  }
/*origin
destinations
direct_flights
departure_date_from
departure_date_to
departure_date_weekend
return_date_from
return_date_to
return_date_weekend
stay_from
stay_to
deals_limit
timelimit*/
  // 2016-05-01  <- dates format
  //-------------------------
  // search itself
  std::vector<deals::DealInfo> result = db.searchForCheapestEver(
      origin, destinations,
      conn.context.http.request.query.params["departure_date_from"],
      conn.context.http.request.query.params["departure_date_to"],
      conn.context.http.request.query.params["return_date_from"],
      conn.context.http.request.query.params["return_date_to"], 
      direct_flights, stops_flights, limit, max_lifetime_sec);

  //------------------------------------
  // prepare response format
  // <-  size_info  -><-     data blocks       ->
  // ↓ size_info block length
  // 11;121;121;45;21;{....},{....},{....},{....}
  //    ↑   ↑   ↑  ↑  each data block length
  std::string sizes = "";

  for (std::vector<deals::DealInfo>::iterator deal = result.begin();
       deal != result.end(); ++deal) {
    sizes += std::to_string(deal->data.size()) + ";";
  }

  uint32_t sizes_strlen = std::to_string(sizes.length()).length();

  // increase size_info in case string length of size_info length will be
  // increased
  // after adding size_info block size (first argument) itself
  if (std::to_string(sizes.length() + sizes_strlen + 1).length() !=
      sizes_strlen) {
    sizes_strlen++;
  }

  std::string size_info =
      std::to_string(sizes.length() + sizes_strlen + 1) + ";" + sizes;
  std::string response = size_info;

  // std::cout << "sizes:" << sizes << " sizes.length:" << sizes.length()
  // << " sizes_strlen:" << sizes_strlen << " size_info:" << size_info;

  //-------------------------
  // write to response deals data
  for (std::vector<deals::DealInfo>::iterator deal = result.begin();
       deal != result.end(); ++deal) {
    response += deal->data;
  }

  http::HttpResponse rq_result(200, "OK");
  rq_result.add_header("Content-Type", "application/octet-stream");
  rq_result.add_header("Content-Length", std::to_string(response.length()));
  rq_result.write(response);
  conn.close(rq_result);
}

/*---------------------------------------------------------
* DealsServer addDeal
*-----------------------------------------------------------*/
void DealsServer::addDeal(Connection& conn) {
  std::string origin =
      utils::toUpperCase(conn.context.http.request.query.params["origin"]);
  std::string destination =
      utils::toUpperCase(conn.context.http.request.query.params["destination"]);

  uint32_t price = 0;
  try {
    price = std::stol(conn.context.http.request.query.params["price"]);
  } catch (std::exception e) {
  }

  bool direct_flight =
      utils::toLowerCase(
          conn.context.http.request.query.params["direct_flight"]) != "false";
  std::string departure_date =
      conn.context.http.request.query.params["departure_date"];
  std::string return_date =
      conn.context.http.request.query.params["return_date"];

  if (origin.length() == 0) {
    conn.close(http::HttpResponse(400, "Bad origin", "Bad origin"));
    return;
  }
  if (destination.length() == 0) {
    conn.close(http::HttpResponse(400, "Bad destination", "Bad destination"));
    return;
  }
  if (price == 0) {
    conn.close(http::HttpResponse(400, "Bad price", "Bad price"));
    return;
  }
  if (deals::utils::date_to_int(departure_date) == 0) {
    conn.close(
        http::HttpResponse(400, "Bad departure_date", "Bad departure_date"));
    return;
  }

  std::string data = conn.context.http.get_body();

  // std::cout << "(add) dep:" << departure_date << " from:" << origin
  //           << " to:" << destination << " ret:" << return_date
  //           << " price:" << price << std::endl;

  db.addDeal(origin, destination, departure_date, return_date, direct_flight,
             price, data);

  conn.close(http::HttpResponse(200, "OK", "Added\n"));
}

}  // deals_srv namespace

// ----------------------------------------------------------
int main(int argc, char* argv[]) {
  if (argc > 1 && std::string(argv[1]) == "test") {
    std::cout << "running autotests..." << std::endl;

    http::unit_test();
    deals::unit_test();
    locks::unit_test();
    timing::unit_test();

    std::cout << "ALL OK" << std::endl;
    return 0;
  }

  if (argc < 2) {
    std::cout << "deals_server <port>" << std::endl;
    return -1;
  }

  uint16_t port = std::stol(argv[1]);

  deals_srv::DealsServer srv(port);

  while (1) {
    srv.process();
  }

  return 0;
}