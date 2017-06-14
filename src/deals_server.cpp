#include <csignal>
#include <fstream>

#include "deals_server.hpp"
#include "locks.hpp"
#include "statsd_client.hpp"
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
  std::cout << "ERROR GOT signal:" << sig << std::endl;
  if (gotQuitSignal) {
    std::cout << "ERROR GOT signal (SECOND TIME):" << sig << " exiting..." << std::endl;
    std::exit(-1);
  }
  gotQuitSignal = true;
}

//-----------------------------------------------------------
// DealsServer process in child class
//-----------------------------------------------------------
void DealsServer::process() {
  auto connections = srv::TCPServer<Context>::process();

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

// TODO
// *) remove truncate methods
// *) overwrite not expired pages on low mem
// *) add some indexes on page index record for search speed up
// *) reduce storage types uint32t -> uint16 (is it worth it? chech DealsData size)
// *) clear mem mechanism parallesation?
// *) logger with date/time
// *) stat info: connections, records count (used/expired/total), opened pages

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
    auto path = conn.context.http.request.query.path;
    if ("GET" == conn.context.http.request.method) {
      if ("/deals/top" == path) {
        getTop(conn);
        return;
      }

      if ("/deals/uniqueRoutes" == path) {
        getUniqueRoutes(conn);
        return;
      }

      if ("/destinations/top" == path) {
        getDestiantionsTop(conn);
        return;
      }

      if ("/deals/clear" == path) {
        db.truncate();
        http::HttpResponse response(200, "OK", "deals cleared\n");
        conn.close(response);
        return;
      }

      if ("/destinations/clear" == path) {
        db_dst.truncate();
        http::HttpResponse response(200, "OK", "destinations cleared\n");
        conn.close(response);
        return;
      }

      if ("/clear" == path) {
        db.truncate();
        db_dst.truncate();
        http::HttpResponse response(200, "OK", "ALL cleared\n");
        conn.close(response);
        return;
      }

      if ("/ping" == path) {
        http::HttpResponse response(200, "OK", "pong\n");
        conn.close(response);
        return;
      }

      if ("/quit" == path) {
        quit();
        http::HttpResponse response(200, "OK", "quiting...\n");
        conn.close(response);
        return;
      }
    } else if ("POST" == conn.context.http.request.method) {
      if ("/deals/add" == path) {
        addDeal(conn);
        return;
      }
    }

    // default response:
    conn.close(http::HttpResponse(404, "Not Found", "Method unknown\n"));

  } catch (types::Error err) {
    terminateWithError(conn, err);
  } catch (...) {
    types::Error err{"Something is broken inside me\n", types::ErrorCode::InternalError};
    terminateWithError(conn, err);
  }
}

//-----------------------------------------------------------
// DealsServer terminateWithError
//-----------------------------------------------------------
void DealsServer::terminateWithError(Connection &conn, types::Error &err) {
  auto ip = conn.context.http.headers["x-real-ip"];
  if (ip.length() == 0) {
    ip = conn.get_client_address();
  }
  std::cerr << ip << " " << conn.context.http.request.uri << " ERROR: " << err.message << std::endl;
  if (err.code == types::ErrorCode::BadParameter) {
    conn.close(http::HttpResponse(400, "Bad request", err.message));
  } else {
    conn.close(http::HttpResponse(500, "Internal Server Error", err.message));
  }
}

//-----------------------------------------------------------
// DealsServer getTop
//-----------------------------------------------------------
void DealsServer::getTop(Connection &conn) {
  auto params = conn.context.http.request.query.params;

  using namespace types;                                                          // Examples:
  Required<IATACode> origin(params, "origin");                                    // MOW
  Optional<IATACodes> destinations(params, "destinations");                       // MAD,BER,LAX,LON
  Optional<CountryCodes> destination_countries(params, "destination_countries");  // RU,ES,DE
  Optional<Date> departure_date_from(params, "departure_date_from");              // 2016-05-01
  Optional<Date> departure_date_to(params, "departure_date_to");                  // 2016-05-01
  Optional<Date> return_date_from(params, "return_date_from");                    // 2016-05-01
  Optional<Date> return_date_to(params, "return_date_to");                        // 2016-05-01
  Optional<Weekdays> dweekdays(params, "departure_days_of_week");                 // sun,mon,fri
  Optional<Weekdays> rweekdays(params, "return_days_of_week");                    // sun,mon,fri
  Optional<Number> stay_from(params, "stay_from");                                // 3
  Optional<Number> stay_to(params, "stay_to");                                    // 10
  Optional<Number> timelimit(params, "timelimit");                                // 10
  Optional<Number> deals_limit(params, "deals_limit");                            // 10
  Optional<Boolean> direct_flights(params, "direct_flights");                     // false
  Optional<Boolean> roundtrip_flights(params, "roundtrip_flights");               // false
  Optional<Boolean> add_locale_top(params, "add_locale_top");                     // false
  Optional<Boolean> group_by_date(params, "group_by_date");                       // false
  Optional<Boolean> group_by_country(params, "group_by_country");                 // false
  Optional<CountryCode> locale(params, "locale");                                 // ru
  Optional<Date> departure_or_return_date(params, "departure_or_return_date");    // 2016-05-01

  // temporary for backward compatibility
  Optional<Boolean> day_by_day(params, "day_by_day");

  if (return_date_from > return_date_to || departure_date_from > departure_date_to ||
      departure_date_from > return_date_from || departure_date_from > return_date_to ||
      departure_date_to > return_date_to) {
    throw types::Error("Bad date parameters in request\n");
  }

  if (add_locale_top.isDefined() && add_locale_top.isTrue()) {
    if (locale.isUndefined()) {
      throw types::Error("No locale provided on add_locale_top=true\n");
    }

    auto result = db_dst.getLocaleTop(locale, departure_date_from, departure_date_to, deals_limit);
    for (const auto &dst : result) {
      destinations.add_code(dst.destination);
    }
  }

#define TOP_SEARCH_PARAMS                                                                         \
  origin, destinations, destination_countries, departure_date_from, departure_date_to, dweekdays, \
      return_date_from, return_date_to, rweekdays, stay_from, stay_to, direct_flights,            \
      deals_limit, timelimit, roundtrip_flights, departure_or_return_date

  if (group_by_date.isDefined() && group_by_date.isTrue()) {
    writeTopResult(conn, db.searchFor<deals::CheapestByDay>(TOP_SEARCH_PARAMS));
  } else if (group_by_country.isDefined() && group_by_country.isTrue()) {
    writeTopResult(conn, db.searchFor<deals::CheapestByCountry>(TOP_SEARCH_PARAMS));
  } else {
    writeTopResult(conn, db.searchFor<deals::SimplyCheapest>(TOP_SEARCH_PARAMS));
  }
}

//------------------------------------------------------------
// DealsServer writeTopResult
//------------------------------------------------------------
void DealsServer::writeTopResult(Connection &conn, const std::vector<deals::DealInfo> &&result) {
  if (result.size() == 0) {
    http::HttpResponse rq_result(204, "Empty result");
    rq_result.add_header("Content-Length", "0");
    conn.close(rq_result);
    return;
  }

  // prepare response format
  // <-  size_info  -><-     data blocks       ->
  // ↓ size_info block length
  // 11;121;121;45;21;{....},{....},{....},{....}
  //    ↑   ↑   ↑  ↑  each data block length
  std::string sizes = "";

  for (const auto &deal : result) {
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

  for (const auto &deal : result) {
    response += deal.data;
  }

  http::HttpResponse rq_result(200, "OK");
  rq_result.add_header("Content-Type", "application/octet-stream");
  rq_result.add_header("Content-Length", std::to_string(response.length()));
  rq_result.write(response);
  conn.close(rq_result);
}

//-----------------------------------------------------------
// getUniqueRoutes
//-----------------------------------------------------------
void DealsServer::getUniqueRoutes(Connection &conn) {
  const auto result = db.getUniqueRoutesDeals();

  if (result.size() == 0) {
    http::HttpResponse rq_result(204, "Empty result");
    rq_result.add_header("Content-Length", "0");
    conn.close(rq_result);
    return;
  }

  http::HttpResponse rq_result(200, "OK");
  rq_result.add_header("Content-Type", "text/plain");
  rq_result.add_header("Content-Length", std::to_string(result.length()));
  rq_result.write(result);
  conn.close(rq_result);
}

//------------------------------------------------------------
// DealsServer addDeal
//------------------------------------------------------------
void DealsServer::addDeal(Connection &conn) {
  auto params = conn.context.http.request.query.params;

  using namespace types;
  Optional<Date> return_date(params, "return_date");
  Required<Date> departure_date(params, "departure_date");
  Required<Number> price(params, "price");
  Required<CountryCode> locale(params, "locale");
  Required<CountryCode> dst_country(params, "destination_country");
  Required<IATACode> origin(params, "origin");
  Required<IATACode> destination(params, "destination");
  Required<Boolean> direct_flight(params, "direct_flight");

  if (origin == destination) {
    throw types::Error("Error origin = destination\n");
  }
  if (return_date.isDefined() && departure_date > return_date) {
    throw types::Error("departure date > return date\n");
  }

  // read request body (packed deal json)
  db.addDeal(origin, destination, dst_country, departure_date, return_date,  //
             direct_flight, price, conn.context.http.get_body());
  db_dst.addDestination(locale, destination, departure_date);

  conn.close(http::HttpResponse(200, "OK", "Well done\n"));
}

/*---------------------------------------------------------
* DealsServer getDestiantionsTop
*-----------------------------------------------------------*/
void DealsServer::getDestiantionsTop(Connection &conn) {
  auto params = conn.context.http.request.query.params;

  using namespace types;                                              // Examples:
  Required<CountryCode> locale(params, "locale");                     // ru
  Optional<Date> departure_date_from(params, "departure_date_from");  // 2016-05-01
  Optional<Date> departure_date_to(params, "departure_date_to");      // 2016-05-01
  Optional<Number> destinations_limit(params, "destinations_limit");  // 10

  auto result =
      db_dst.getLocaleTop(locale, departure_date_from, departure_date_to, destinations_limit);

  if (result.size() == 0) {
    http::HttpResponse rq_result(204, "empty result");
    rq_result.add_header("Content-Length", "0");
    conn.close(rq_result);
    return;
  }

  std::string response;
  for (const auto &elm : result) {
    response += types::code_to_origin(elm.destination) + ";" + std::to_string(elm.counter) + "\n";
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
    try {
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
    } catch (types::Error err) {
      std::cerr << "ERRORS FOUND IN TEST: " << err.message << std::endl;
      return -1;
    } catch (char const *d) {
      std::cerr << "ERRORS FOUND IN TEST: " << d << std::endl;
      return -1;
    } catch (...) {
      std::cerr << "ERRORS FOUND IN TEST: " << std::endl;
      return -1;
    }
  }

  if (argc > 1 && std::string(argv[1]) == "stat") {
    statsd::metric.inc("dealstest", {{"port", "5000"}});
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