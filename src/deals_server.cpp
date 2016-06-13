#include "deals_server.hpp"
#include <fstream>

namespace deals_srv {

/* --------------------------------------------------------
* DealsServer on_connect
*----------------------------------------------------------*/
void DealsServer::on_connect(Connection& conn) {
  // std::cout << "new conn:" << conn.get_client_ip() << std::endl;
}

/* --------------------------------------------------------
* DealsServer on_connect
*----------------------------------------------------------*/
void DealsServer::on_data(Connection& conn) {
  conn.context.http.write(conn.get_data());

  if (!conn.context.http.is_request_complete()) {
    return;
  }

  if (conn.context.http.request.method == "GET") {
    if (conn.context.http.request.query.path == "/deals/add") {
      addDeal(conn);
      return;
    }

    if (conn.context.http.request.query.path == "/quit") {
      http::HttpResponse response(200, "OK", "quiting...\n");
      conn.close(response);
      return;
    }
  }

  // default:
  conn.close(http::HttpResponse (404, "Not Found", "Method unknown\n"));

  // std::ofstream myfile ("/tmp/example.bin", std::ios::out /*| std::ios::app*/
  // | std::ios::binary);
  // myfile << conn.context.http.get_body();
  // myfile.close();
  // std::cout << "file written\n";
}

void DealsServer::addDeal(Connection& conn) {
  std::string origin = conn.context.http.request.query.params["origin"];
  std::string destination =
      conn.context.http.request.query.params["destination"];
  std::string price = conn.context.http.request.query.params["price"];

  if (origin.length() == 0) {
    conn.close(http::HttpResponse(400, "No origin", "No origin"));
    return;
  }
  if (destination.length() == 0) {
    conn.close(http::HttpResponse(400, "No destination", "No destination"));
    return;
  }
  if (price.length() == 0) {
    conn.close(http::HttpResponse(400, "No price", "No price"));
    return;
  }

  uint32_t price_int = 0;
  try {
    price_int = std::stol(price);
  } catch (std::exception e) {
    conn.close(http::HttpResponse(400, "Price not a number", "Price not a number"));
    return;
  }

  std::string data = origin + destination + price;

  std::cout << "(add) from:" << origin << " to:" << destination << std::endl;
  db.addDeal(origin, destination, timing::getTimestampSec(),
             timing::getTimestampSec(), true, price_int,
             (deals::DealData*)data.c_str(), data.length());

  conn.close(http::HttpResponse(200, "OK", "added\n"));
}

}  // deals_srv namespace

int main() {
  deals_srv::DealsServer srv(5000);

  while (1) {
    srv.process();
  }

  return 0;
}