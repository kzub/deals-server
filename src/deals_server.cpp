#include "deals_server.hpp"
#include <fstream>

namespace deals_srv {

/* --------------------------------------------------------
* DealsServer on connect
*----------------------------------------------------------*/
void DealsServer::on_connect(Connection& conn) {
  // std::cout << "new conn:" << conn.get_client_ip() << std::endl;
}

/* --------------------------------------------------------
* DealsServer on data
*----------------------------------------------------------*/
void DealsServer::on_data(Connection& conn) {
  conn.context.http.write(conn.get_data());

  if (!conn.context.http.is_request_complete()) {
    return;
  }

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
  } else if (conn.context.http.request.method == "POST") {
    if (conn.context.http.request.query.path == "/deals/add") {
      addDeal(conn);
      return;
    }
  }

  // default:
  conn.close(http::HttpResponse(404, "Not Found", "Method unknown\n"));

  // std::ofstream myfile ("/tmp/example.bin", std::ios::out /*| std::ios::app*/
  // | std::ios::binary);
  // myfile << conn.context.http.get_body();
  // myfile.close();
  // std::cout << "file written\n";
}

/*---------------------------------------------------------
* DealsServer getTop
*-----------------------------------------------------------*/
void DealsServer::getTop(Connection& conn) {

  std::string origin =
      utils::toUpperCase(conn.context.http.request.query.params["origin"]);
  std::string destinations = utils::toUpperCase(
      conn.context.http.request.query.params["destinations"]);

  if (origin.length() == 0) {
    conn.close(http::HttpResponse(400, "No origin", "No origin"));
    return;
  }


  std::vector<deals::DealInfo> result =
      db.searchForCheapestEver(origin, destinations, 120);
  
  std::string sizes = "";

  for (std::vector<deals::DealInfo>::iterator deal = result.begin();
       deal != result.end(); ++deal) {
    sizes += std::to_string(deal->data.size()) + ";";
  }

  // <-  size_info  -><-     data blocks       ->
  // ↓ size_info block length
  // 11;121;121;45;21;{....},{....},{....},{....}
  //    ↑   ↑   ↑  ↑  each data block length

  uint32_t sizes_strlen = std::to_string(sizes.length()).length();

  // increase size_info in case string length of size_info length will be increased
  // after adding size_info block size (first argument) itself 
  if(std::to_string(sizes.length() + sizes_strlen + 1).length() != sizes_strlen){
    sizes_strlen++;
  }

  std::string size_info = std::to_string(sizes.length() + sizes_strlen + 1) + ";" + sizes;
  std::string response = size_info;

  // std::cout << "sizes:" << sizes << " sizes.length:" << sizes.length() 
            // << " sizes_strlen:" << sizes_strlen << " size_info:" << size_info;

  for (std::vector<deals::DealInfo>::iterator deal = result.begin();
       deal != result.end(); ++deal) {
    response += deal->data;
  }

  conn.close(http::HttpResponse(200, "OK", response));
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

  uint32_t departure_date = deals::utils::date_to_int(
      conn.context.http.request.query.params["departure_date"]);
  uint32_t return_date = deals::utils::date_to_int(
      conn.context.http.request.query.params["return_date"]);
  bool direct_flight =
      conn.context.http.request.query.params["direct_flight"] != "false";

  if (origin.length() == 0) {
    conn.close(http::HttpResponse(400, "No origin", "No origin"));
    return;
  }
  if (destination.length() == 0) {
    conn.close(http::HttpResponse(400, "No destination", "No destination"));
    return;
  }
  if (price == 0) {
    conn.close(http::HttpResponse(400, "No price", "No price"));
    return;
  }
  if (departure_date == 0) {
    conn.close(
        http::HttpResponse(400, "No departure_date", "No departure_date"));
    return;
  }

  std::string data = conn.context.http.get_body();

  // std::cout << "(add) dep:" << departure_date << " from:" << origin
  //           << " to:" << destination << " ret:" << return_date
  //           << " price:" << price << std::endl;

  db.addDeal(origin, destination, departure_date, return_date, direct_flight,
             price, data);

  conn.close(http::HttpResponse(200, "OK", "added\n"));
}

}  // deals_srv namespace

// ----------------------------------------------------------
int main() {
  deals_srv::DealsServer srv(5000);

  while (1) {
    srv.process();
  }

  return 0;
}