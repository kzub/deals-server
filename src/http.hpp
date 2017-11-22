#ifndef SRC_HTTP_PARSER_HPP
#define SRC_HTTP_PARSER_HPP

#include <iostream>
#include <vector>
#include "utils.hpp"

namespace http {

enum class ParserResult : int { PARSE_OK = 0, PARSE_AWAIT = 1, PARSE_ERR = -1 };

// ------------------------------------------------------------------
class HttpHeaders : public types::ObjectMap {
 public:
  ParserResult parse(std::string http_message);
};

/*------------------------------------------------------------------
* Query params parser and storage
------------------------------------------------------------------*/
class URIQueryParams {
 public:
  ParserResult parse(std::string query_text);
  types::ObjectMap params;
  std::string path;
};

/*------------------------------------------------------------------
* Request line parser and storage
------------------------------------------------------------------*/
class HttpRequest {
 public:
  ParserResult parse(std::string requestline);

  std::string method;
  std::string uri;
  std::string http_version;
  URIQueryParams query;
};

class HttpParser;
class HttpResponse;

/*------------------------------------------------------------------
* Request parser
------------------------------------------------------------------*/
class HttpParser {
 public:
  void write(const char* data, size_t size);
  void write(const std::string& msg);
  bool is_request_complete();
  bool is_bad_request();
  bool is_headers_complete();

  // result data:
  std::string get_body();
  std::string get_headers();
  std::string get_request_line();

  HttpHeaders headers;
  HttpRequest request;

 private:
  std::vector<std::string> msgs;
  size_t headers_end = 0;
  size_t bytes_written = 0;
  size_t content_length = 0;
  bool headers_written = false;
  bool parsing_complete = false;
  bool bad_request = false;
};

/*------------------------------------------------------------------
* HttpResponse
------------------------------------------------------------------*/
class HttpResponse {
 public:
  HttpResponse(uint16_t status_code, std::string reason_phrase);
  HttpResponse(uint16_t status_code, std::string reason_phrase, std::string body);

  void add_header(std::string name, std::string value);
  void write(const std::string& msg);

  operator std::string();

 private:
  uint16_t status_code;
  std::string reason_phrase;
  std::vector<std::string> headers;
  std::vector<std::string> body;
};

void unit_test();
}  // namespace http

#endif