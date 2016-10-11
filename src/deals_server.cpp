#include <csignal>
#include <fstream>

#include "deals_server.hpp"
#include "locks.hpp"
#include "timing.hpp"

namespace deals_srv {

//-----------------------------------------------------------
// DealsServer on connect
//-----------------------------------------------------------
void DealsServer::on_connect(Connection &conn) {
  // std::cout << "new conn:" << conn.get_client_address() << std::endl;
}

//-----------------------------------------------------------
// DealsServer quit request inside application
//-----------------------------------------------------------
void DealsServer::quit() {
  std::cout << "WARNING DealsServer::quit()" << std::endl;
  quit_request = true;
}

//-----------------------------------------------------------
// system signals handler
//-----------------------------------------------------------
bool gotQuitSignal = false;
void signalHandler(int signal) {
  std::string sig;
  switch (signal) {
    case SIGINT:
      sig = "SIGINT";
      break;
    case SIGTERM:
      sig = "SIGTERM";
      break;
    case SIGBUS:
      sig = "SIGBUS";
      break;
    default:
      sig = std::to_string(signal);
      break;
  }
  std::cout << "GOT signal:" << sig << std::endl;
  if (gotQuitSignal) {
    std::cout << "SECOND TIME signal:" << sig << " exiting..." << std::endl;
    std::exit(-1);
  }
  gotQuitSignal = true;
}

//-----------------------------------------------------------
// DealsServer process in child class
//-----------------------------------------------------------
void DealsServer::process() {
  uint16_t connections = srv::TCPServer<Context>::process();

  // quit after all connections are closed
  if (gotQuitSignal) {
    gotQuitSignal = false;
    quit();
  }

  if (quit_request) {
    std::cout << "Waiting for connections... " << connections << std::endl;
    if (connections == 0) {
      std::cout << "No active connections -> quit!" << std::endl;
      std::exit(0);
    }
  }
}

// --------------------------------------------------------
// DealsServer on data
//-----------------------------------------------------------
void DealsServer::on_data(Connection &conn) {
  // gracefull restart
  if (quit_request) {
    http::HttpResponse response(503, "Service unavailable", "Service unavailable\n");
    conn.close(response);
    return;
  }

  conn.context.http.write(conn.get_data());

  if (!conn.context.http.is_request_complete()) {
    return;
  }

  // try to process http request
  try {
    if (conn.context.http.request.method == "GET") {
      //-------------------  'GET' BEGIN ----------------------
      if (conn.context.http.request.query.path == "/deals/top") {
        getTop(conn);
        return;
      }
      //--------
      if (conn.context.http.request.query.path == "/destinations/top") {
        getDestiantionsTop(conn);
        return;
      }
      //--------
      if (conn.context.http.request.query.path == "/deals/clear") {
        db.truncate();
        http::HttpResponse response(200, "OK", "deals cleared\n");
        conn.close(response);
        return;
      }
      //--------
      if (conn.context.http.request.query.path == "/destinations/clear") {
        db_dst.truncate();
        http::HttpResponse response(200, "OK", "destinations cleared\n");
        conn.close(response);
        return;
      }
      //--------
      if (conn.context.http.request.query.path == "/clear") {
        db.truncate();
        db_dst.truncate();
        http::HttpResponse response(200, "OK", "ALL cleared\n");
        conn.close(response);
        return;
      }
      //--------
      if (conn.context.http.request.query.path == "/ping") {
        http::HttpResponse response(200, "OK", "pong\n");
        conn.close(response);
        return;
      }
      //--------
      if (conn.context.http.request.query.path == "/quit") {
        quit();
        http::HttpResponse response(200, "OK", "quiting...\n");
        conn.close(response);
        return;
      }

      // TODO
      // *) clear mem mechanism parallesation?
      // *) check day by day logic in case of no destinations specified. look for opt opportunities
      // *) overwrite not expired pages on low mem
      // *) add strong types for function parameters
      // *) unit test + alg speed comparasion of DealsCheapestDayByDay::process_deal
      // *) increase expire time to 2 days. but search for deals just for 8 hours by default
      // *) logger with date/time
      // *) stat info: connections, records count (used/expired/total), opened pages
      // *) nginx cache for requests
      // *) (+) check if price is less than top N max. Otherwise skip map[destination] calculation
      // *) (+) add cache for getLocaleTop
      // *) (+) rewrite to std::unordered_map grouping (month calendar)
      // *) (+) stop accepting new connections and quit after all connections are served
      // *) (+) calc amount of space left on dev/shm
      // *) (+) OW filter
      // *) (+) rewrite sym waiting func
      // *) ? Deals By Aircompany
      // -------------------  'GET' END ----------------------
    } else if (conn.context.http.request.method == "POST") {
      //-------------------  'POST' BEGIN ----------------------
      if (conn.context.http.request.query.path == "/deals/add") {
        addDeal(conn);
        return;
      }
    }  //-------------------  'POST' END ----------------------

    // default response:
    conn.close(http::HttpResponse(404, "Not Found", "Method unknown\n"));

  } catch (deals::RequestError err) {
    std::cout << "Request error: " << err.message << std::endl;
    conn.close(http::HttpResponse(err.code, "Internal Server Error", err.message));
  } catch (const char *text) {
    std::cout << "Request error: " << text << std::endl;
    conn.close(http::HttpResponse(500, "Internal Server Error", text));
  } catch (...) {
    std::cout << "Request processing error: unknown" << std::endl;
    conn.close(
        http::HttpResponse(500, "Internal Server Error", "something has broken inside me\n"));
  }
}

//-----------------------------------------------------------
// DealsServer getTop
//-----------------------------------------------------------
void DealsServer::getTop(Connection &conn) {
  // origin
  //-------------------------
  std::string origin = utils::toUpperCase(conn.context.http.request.query.params["origin"]);
  if (origin.length() != 3) {
    conn.close(http::HttpResponse(400, "Bad origin", "Bad origin\n"));
    return;
  }

  // destinations
  //-------------------------
  std::string destinations =
      utils::toUpperCase(conn.context.http.request.query.params["destinations"]);
  if (!query::check_destinations_format(destinations)) {
    conn.close(http::HttpResponse(400, "Bad destinations", "Bad destinations\n"));
    return;
  }

  // direct_flights
  //-------------------------
  utils::Threelean direct_flights = utils::Threelean::Undefined;

  std::string direct = utils::toLowerCase(conn.context.http.request.query.params["direct_flights"]);
  if (direct == "true") {
    direct_flights = utils::Threelean::True;
  } else if (direct == "false") {
    direct_flights = utils::Threelean::False;
  } else if (direct != "") {
    conn.close(http::HttpResponse(400, "Bad direct_flights", "Bad direct_flights\n"));
    return;
  }

  // roundtrip_flights
  //-------------------------
  utils::Threelean roundtrip_flights = utils::Threelean::Undefined;

  std::string rt = utils::toLowerCase(conn.context.http.request.query.params["roundtrip_flights"]);
  if (rt == "true") {
    roundtrip_flights = utils::Threelean::True;
  } else if (rt == "false") {
    roundtrip_flights = utils::Threelean::False;
  } else if (rt != "") {
    conn.close(http::HttpResponse(400, "Bad roundtrip_flights", "Bad roundtrip_flights\n"));
    return;
  }

  // timelimit
  //-------------------------
  uint32_t max_lifetime_sec = 0;
  if (conn.context.http.request.query.params["timelimit"].length()) {
    try {
      max_lifetime_sec = std::stol(conn.context.http.request.query.params["timelimit"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad timelimit", "Bad timelimit\n"));
      return;
    }
  }

  // deals_limit
  //-------------------------
  uint16_t limit = 0;
  if (conn.context.http.request.query.params["deals_limit"].length()) {
    try {
      limit = std::stol(conn.context.http.request.query.params["deals_limit"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad deals_limit", "Bad deals_limit\n"));
      return;
    }
  }

  // stay_from
  //-------------------------
  uint16_t stay_from = 0;
  if (conn.context.http.request.query.params["stay_from"].length()) {
    try {
      stay_from = std::stol(conn.context.http.request.query.params["stay_from"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad stay_from", "Bad stay_from\n"));
      return;
    }
  }

  // stay_to
  //-------------------------
  uint16_t stay_to = 0;
  if (conn.context.http.request.query.params["stay_to"].length()) {
    try {
      stay_to = std::stol(conn.context.http.request.query.params["stay_to"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad stay_to", "Bad stay_to\n"));
      return;
    }
  }

  // departure_days_of_week
  //-------------------------
  std::string dweekdays = conn.context.http.request.query.params["departure_days_of_week"];
  if (dweekdays.length() && !query::check_weekdays_format(dweekdays)) {
    conn.close(
        http::HttpResponse(400, "Bad departure_days_of_week", "Bad departure_days_of_week\n"));
    return;
  }

  // return_days_of_week
  //-------------------------
  std::string rweekdays = conn.context.http.request.query.params["return_days_of_week"];
  if (rweekdays.length() && !query::check_weekdays_format(rweekdays)) {
    conn.close(http::HttpResponse(400, "Bad return_days_of_week", "Bad return_days_of_week\n"));
    return;
  }

  // departure_date_from     ( date format: 2016-05-01 )
  //-------------------------
  std::string departure_date_from = conn.context.http.request.query.params["departure_date_from"];
  if (departure_date_from.length() && !query::check_date_format(departure_date_from)) {
    conn.close(http::HttpResponse(400, "Bad departure_date_from", "Bad departure_date_from\n"));
    return;
  }

  // departure_date_to       ( date format: 2016-05-01 )
  //-------------------------
  std::string departure_date_to = conn.context.http.request.query.params["departure_date_to"];
  if (departure_date_to.length() && !query::check_date_format(departure_date_to)) {
    conn.close(http::HttpResponse(400, "Bad departure_date_to", "Bad departure_date_to\n"));
    return;
  }

  // return_date_from        ( date format: 2016-05-01 )
  //-------------------------
  std::string return_date_from = conn.context.http.request.query.params["return_date_from"];
  if (return_date_from.length() && !query::check_date_format(return_date_from)) {
    conn.close(http::HttpResponse(400, "Bad return_date_from", "Bad return_date_from\n"));
    return;
  }

  // return_date_to          ( date format: 2016-05-01 )
  //-------------------------
  std::string return_date_to = conn.context.http.request.query.params["return_date_to"];
  if (return_date_to.length() && !query::check_date_format(return_date_to)) {
    conn.close(http::HttpResponse(400, "Bad return_date_to", "Bad return_date_to\n"));
    return;
  }

  // check departure dates < return dates
  //-------------------------
  if (!query::check_date_to_date(return_date_from, return_date_to) ||
      !query::check_date_to_date(departure_date_from, departure_date_to) ||
      !query::check_date_to_date(departure_date_from, return_date_from) ||
      !query::check_date_to_date(departure_date_from, return_date_to) ||
      !query::check_date_to_date(departure_date_to, return_date_to)) {
    conn.close(http::HttpResponse(400, "Bad date parameters", "Bad date parameters\n"));
    return;
  }

  // price_from
  //-------------------------
  uint32_t price_from = 0;
  if (conn.context.http.request.query.params["price_from"].length()) {
    try {
      price_from = std::stol(conn.context.http.request.query.params["price_from"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad price_from", "Bad price_from\n"));
      return;
    }
  }

  // price_to
  //-------------------------
  uint32_t price_to = 0;
  if (conn.context.http.request.query.params["price_to"].length()) {
    try {
      price_to = std::stol(conn.context.http.request.query.params["price_to"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad price_to", "Bad price_to\n"));
      return;
    }
  }

  // add_locale_top
  //-------------------------
  if (conn.context.http.request.query.params["add_locale_top"] == "true") {
    std::string locale = conn.context.http.request.query.params["locale"];
    if (locale.length() != 2) {
      conn.close(http::HttpResponse(400, "Bad locale", "Bad locale\n"));
      return;
    }

    std::vector<top::DstInfo> result =
        db_dst.getLocaleTop(locale, departure_date_from, departure_date_to, limit);

    for (auto &dst : result) {
      if (destinations.length() != 0) {
        destinations += ",";
      }
      destinations += query::code_to_origin(dst.destination);
    }
  }

  // all check are done. Input seems to be fine
  // SEARCH STEP
  //-------------------------
  std::vector<deals::DealInfo> result;

  if (conn.context.http.request.query.params["day_by_day"] == "true") {
    result = db.searchForCheapestDayByDay(
        origin, destinations, departure_date_from, departure_date_to, dweekdays, return_date_from,
        return_date_to, rweekdays, stay_from, stay_to, direct_flights, price_from, price_to, limit,
        max_lifetime_sec, roundtrip_flights);
  } else {
    result = db.searchForCheapest(origin, destinations, departure_date_from, departure_date_to,
                                  dweekdays, return_date_from, return_date_to, rweekdays, stay_from,
                                  stay_to, direct_flights, price_from, price_to, limit,
                                  max_lifetime_sec, roundtrip_flights);
  }

  // No results
  //-------------------------
  if (result.size() == 0) {
    http::HttpResponse rq_result(204, "empty result");
    rq_result.add_header("Content-Length", "0");
    conn.close(rq_result);
    return;
  }

  //------------------------------------
  // prepare response format
  // <-  size_info  -><-     data blocks       ->
  // ↓ size_info block length
  // 11;121;121;45;21;{....},{....},{....},{....}
  //    ↑   ↑   ↑  ↑  each data block length
  std::string sizes = "";

  for (auto &deal : result) {
    sizes += std::to_string(deal.data.size()) + ";";
  }

  uint32_t sizes_strlen = std::to_string(sizes.length()).length();

  // increase size_info in case string length of size_info length will be increased
  // after adding size_info block size (first argument) itself
  if (std::to_string(sizes.length() + sizes_strlen + 1).length() != sizes_strlen) {
    sizes_strlen++;
  }

  std::string size_info = std::to_string(sizes.length() + sizes_strlen + 1) + ";" + sizes;
  std::string response = size_info;

  // write to response deals data
  //-------------------------
  for (auto &deal : result) {
    response += deal.data;
  }

  http::HttpResponse rq_result(200, "OK");
  rq_result.add_header("Content-Type", "application/octet-stream");
  rq_result.add_header("Content-Length", std::to_string(response.length()));
  rq_result.write(response);
  conn.close(rq_result);
}

//------------------------------------------------------------
// DealsServer addDeal
//------------------------------------------------------------
void DealsServer::addDeal(Connection &conn) {
  // locale
  //------------
  std::string locale = utils::toLowerCase(conn.context.http.request.query.params["locale"]);
  if (locale.length() != 2) {
    conn.close(http::HttpResponse(400, "Bad locale", "Bad locale"));
    return;
  }

  // origin
  //------------
  std::string origin = utils::toUpperCase(conn.context.http.request.query.params["origin"]);
  if (origin.length() != 3) {
    conn.close(http::HttpResponse(400, "Bad origin", "Bad origin"));
    return;
  }

  // destinations
  //-------------
  std::string destination =
      utils::toUpperCase(conn.context.http.request.query.params["destination"]);
  if (destination.length() != 3) {
    conn.close(http::HttpResponse(400, "Bad destination", "Bad destination"));
    return;
  }

  if (origin == destination) {
    conn.close(http::HttpResponse(400, "Bad origin eq destination", "Bad origin eq destination"));
    return;
  }

  // price
  //-------------
  uint32_t price = 0;
  try {
    price = std::stol(conn.context.http.request.query.params["price"]);
  } catch (...) {
    conn.close(http::HttpResponse(400, "Bad price", "Bad price"));
    return;
  }

  // direct_flight
  //-------------
  std::string direct_flight_str =
      utils::toLowerCase(conn.context.http.request.query.params["direct_flight"]);
  if (direct_flight_str != "true" && direct_flight_str != "false") {
    conn.close(http::HttpResponse(400, "Bad direct_flight", "Bad direct_flight"));
    return;
  }
  bool direct_flight = direct_flight_str == "true";

  // departure_date
  //-------------
  std::string departure_date = conn.context.http.request.query.params["departure_date"];
  if (query::date_to_int(departure_date) == 0) {
    conn.close(http::HttpResponse(400, "Bad departure_date", "Bad departure_date"));
    return;
  }

  // return_date
  //-------------
  std::string return_date = conn.context.http.request.query.params["return_date"];
  if (return_date.length() > 0) {
    if (query::date_to_int(return_date) == 0) {
      conn.close(http::HttpResponse(400, "Bad return_date", "Bad return_date"));
      return;
    }
  }

  if (!query::check_date_to_date(departure_date, return_date)) {
    conn.close(http::HttpResponse(400, "Bad date parameters", "Bad date parameters\n"));
    return;
  }

  // read POST body (zipped deal json)
  std::string data = conn.context.http.get_body();

  // std::cout << "(add) dep:" << departure_date << " from:" << origin
  //           << " to:" << destination << " ret:" << return_date
  //           << " price:" << price << std::endl;

  // deals db
  //---------------
  bool good =
      db.addDeal(origin, destination, departure_date, return_date, direct_flight, price, data);

  if (!good) {
    conn.close(http::HttpResponse(500, "Could not addDeal", "Could not addDeal\n"));
    return;
  }

  // destinations db
  //---------------
  good = db_dst.addDestination(locale, destination, departure_date);

  if (!good) {
    conn.close(http::HttpResponse(500, "Could not addDestination", "Could not addDestination\n"));
    return;
  }

  conn.close(http::HttpResponse(200, "OK", "Added\n"));
}

/*---------------------------------------------------------
* DealsServer getDestiantionsTop
*-----------------------------------------------------------*/
void DealsServer::getDestiantionsTop(Connection &conn) {
  // locale
  //---------------
  std::string locale = utils::toLowerCase(conn.context.http.request.query.params["locale"]);
  if (locale.length() != 2) {
    conn.close(http::HttpResponse(400, "Bad locale", "Bad locale"));
    return;
  }

  // departure_date_from     ( date format: 2016-05-01 )
  //-------------------------
  std::string departure_date_from = conn.context.http.request.query.params["departure_date_from"];
  if (departure_date_from.length() && departure_date_from.length() &&
      !query::check_date_format(departure_date_from)) {
    conn.close(http::HttpResponse(400, "Bad departure_date_from", "Bad departure_date_from\n"));
    return;
  }

  // departure_date_to       ( date format: 2016-05-01 )
  //-------------------------
  std::string departure_date_to = conn.context.http.request.query.params["departure_date_to"];
  if (departure_date_to.length() && departure_date_to.length() &&
      !query::check_date_format(departure_date_to)) {
    conn.close(http::HttpResponse(400, "Bad departure_date_to", "Bad departure_date_to\n"));
    return;
  }

  // timelimit
  //-------------------------
  uint32_t max_lifetime_sec = 0;
  if (conn.context.http.request.query.params["timelimit"].length()) {
    try {
      max_lifetime_sec = std::stol(conn.context.http.request.query.params["timelimit"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad timelimit", "Bad timelimit\n"));
      return;
    }
  }

  // destinations_limit
  //-------------------------
  uint16_t limit = 0;
  if (conn.context.http.request.query.params["destinations_limit"].length()) {
    try {
      limit = std::stol(conn.context.http.request.query.params["destinations_limit"]);
    } catch (...) {
      conn.close(http::HttpResponse(400, "Bad destinations_limit", "Bad destinations_limit\n"));
      return;
    }
  }

  // SEARCH
  //-------------------------
  std::vector<top::DstInfo> result =
      db_dst.getLocaleTop(locale, departure_date_from, departure_date_to, limit);

  // No results
  //-------------------------
  if (result.size() == 0) {
    http::HttpResponse rq_result(204, "empty result");
    rq_result.add_header("Content-Length", "0");
    conn.close(rq_result);
    return;
  }

  // write to response  data
  //-------------------------
  std::string response;
  for (auto &elm : result) {
    response += query::code_to_origin(elm.destination) + ";" + std::to_string(elm.counter) + "\n";
  }

  http::HttpResponse rq_result(200, "OK");
  rq_result.add_header("Content-Type", "text/plain");
  rq_result.add_header("Content-Length", std::to_string(response.length()));
  rq_result.write(response);
  conn.close(rq_result);
}

}  // namespace deals_srv

//-------------------------------------------------
// Here it begins
//--------------------------------------------------
int main(int argc, char *argv[]) {
  if (argc > 1 && std::string(argv[1]) == "test") {
    std::cout << "running autotests..." << std::endl;

    locks::CriticalSection lock1("DealsInfo");
    locks::CriticalSection lock2("DealsData");
    locks::CriticalSection lock3("TopDst");
    lock1.reset_not_for_production();
    lock2.reset_not_for_production();
    lock3.reset_not_for_production();

    http::unit_test();
    deals::unit_test();
    timing::unit_test();
    locks::unit_test();

    std::cout << "ALL OK" << std::endl;
    return 0;
  }

  if (argc < 3) {
    std::cout << "deals_server <host> <port>" << std::endl;
    return -1;
  }

  std::signal(SIGINT, deals_srv::signalHandler);
  std::signal(SIGTERM, deals_srv::signalHandler);
  std::signal(SIGBUS, deals_srv::signalHandler);

  const std::string host = argv[1];
  const uint16_t port = std::stol(argv[2]);
  deals_srv::DealsServer srv(host, port);

  while (1) {
    srv.process();
  }

  return 0;
}