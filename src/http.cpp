
#include <cassert>
#include <cinttypes>
#include <cstring>

#include "http.hpp"
#include "utils.hpp"

namespace http {

// ------------------------------------------------------------------
// parse incoming text and if double CRLF found
// than process and save headers
// ------------------------------------------------------------------
ParserResult HttpHeaders::parse(std::string http_message) {
  if (!http_message.length()) {
    return ParserResult::PARSE_ERR;
  }

  size_t pos = http_message.find("\r\n\r\n");
  if (pos == -1) {
    return ParserResult::PARSE_AWAIT;
  }

  // headers are fully loaded. let we parse it
  std::string http_headers = http_message.substr(0, pos);
  std::vector<std::string> http_headers_lines = utils::split_string(http_headers, "\r\n");

  if (http_headers_lines.size() == 0) {
    return ParserResult::PARSE_ERR;
  }

  // std::cout << "RequestLine:" << http_headers_lines[0] << std::endl;
  // omit first one as it is a request line
  for (auto line = http_headers_lines.begin() + 1; line != http_headers_lines.end(); ++line) {
    // std::cout << "HeaderLine:" << *line << std::endl;
    // split for name & value
    size_t hpos = line->find(":");
    if (hpos == -1) {
      // unknown or wrong header format
      continue;
    }

    types::Object one_header;
    one_header.name = line->substr(0, hpos);
    one_header.name = utils::toLowerCase(one_header.name);

    // remove space if it exists after ':'
    if ((*line)[hpos + 1] == ' ') {
      hpos++;
    }
    one_header.value = line->substr(hpos + 1);

    add_object(one_header);
  }

  return ParserResult::PARSE_OK;
}

//------------------------------------------------------------------
// Query params parser
//------------------------------------------------------------------
ParserResult URIQueryParams::parse(std::string query_text) {
  if (!query_text.length()) {
    return ParserResult::PARSE_ERR;
  }

  size_t pos = query_text.find("?");
  if (pos == -1) {
    path = query_text;
    return ParserResult::PARSE_OK;
  }

  path = query_text.substr(0, pos);

  // separate params strings by '&' char
  std::vector<std::string> query_params = utils::split_string(query_text.substr(pos + 1), "&");

  // for every param 'param1=value' make an object
  for (auto& param : query_params) {
    types::Object one_param;
    // std::cout << "param;" << *param << std::endl;
    size_t pos = param.find("=");
    if (pos == -1) {
      one_param.name = param;
    } else {
      one_param.name = param.substr(0, pos);
      one_param.value = param.substr(pos + 1);
    }

    params.add_object(one_param);
  }

  return ParserResult::PARSE_OK;
};

//------------------------------------------------------------------
// Request parser
//------------------------------------------------------------------
ParserResult HttpRequest::parse(std::string requestline) {
  if (!requestline.length()) {
    return ParserResult::PARSE_ERR;
  }

  size_t pos = requestline.find("\r\n");
  if (pos == -1) {
    return ParserResult::PARSE_ERR;
  }

  /* Request-Line   = Method SP Request-URI SP HTTP-Version CRLF */
  std::vector<std::string> res = utils::split_string(requestline.substr(0, pos), " ");
  if (res.size() != 3) {
    return ParserResult::PARSE_ERR;
  }

  method = res[0];
  uri = res[1];
  http_version = res[2];

  return query.parse(uri);
}

//------------------------------------------------------------------
// HttpParser is_request_complete
//------------------------------------------------------------------
bool HttpParser::is_request_complete() {
  return parsing_complete;
}

//------------------------------------------------------------------
// HttpParser is_bad_request
//------------------------------------------------------------------
bool HttpParser::is_bad_request() {
  return bad_request;
}

//------------------------------------------------------------------
// HttpParser is_headers_complete
//------------------------------------------------------------------
bool HttpParser::is_headers_complete() {
  return headers_written;
}

//------------------------------------------------------------------
// Process network data
//------------------------------------------------------------------
// HTTP REQUEST FORMAT:
// Request       = Request-Line              ; Section 5.1
//                 *(( general-header        ; Section 4.5
//                  | request-header         ; Section 5.3
//                  | entity-header ) CRLF)  ; Section 7.1
//                 CRLF
//                 [ message-body ]          ; Section 4.3
// Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
// ------------------------------------------------------------------
void HttpParser::write(const char* data, size_t size) {
  std::string msg;
  msg.append(data, size);
  write(msg);
};

// save function worked with std::string
void HttpParser::write(const std::string& msg) {
  msgs.push_back(msg);
  bytes_written += msg.length();

  if (!headers_written) {
    std::string concated_msg = utils::concat_string(msgs);

    // check if header are fully downloaded
    size_t pos = concated_msg.find("\r\n\r\n");
    if (pos == -1) {
      return;
    }

    if (headers.parse(concated_msg) != ParserResult::PARSE_OK) {
      return;
    }

    headers_end = pos + 4;
    headers_written = true;

    if (headers["content-length"].length()) {
      try {
        content_length = std::stol(headers["content-length"]);
      } catch (std::exception e) {
      }
    }

    // check HTTP Request format
    auto res = request.parse(concated_msg);
    if (res != http::ParserResult::PARSE_OK) {
      bad_request = true;
      return;
    }

    if (request.method == "GET") {
      parsing_complete = true;
    }
  }

  if (request.method == "POST") {
    if ((bytes_written - headers_end) < content_length) {
      return;
    }
    parsing_complete = true;
  }
}

//------------------------------------------------------------------
// return HTTP body
//------------------------------------------------------------------
std::string HttpParser::get_body() {
  return utils::concat_string(msgs).substr(headers_end);
}

//------------------------------------------------------------------
// Return HTTP Headers
//------------------------------------------------------------------
std::string HttpParser::get_headers() {
  return utils::concat_string(msgs).substr(0, headers_end);
}

//------------------------------------------------------------------
// return Request String
//------------------------------------------------------------------
std::string HttpParser::get_request_line() {
  auto headers = get_headers();
  auto pos = headers.find("\r\n");
  return headers.substr(0, pos);
}

//******************* RESPONSE ****************************
HttpResponse::HttpResponse(uint16_t status_code, std::string reason_phrase)
    : status_code(status_code), reason_phrase(reason_phrase){};

HttpResponse::HttpResponse(uint16_t status_code, std::string reason_phrase, std::string _body)
    : status_code(status_code), reason_phrase(reason_phrase) {
  body.push_back(_body);
};

//------------------------------------------------------------------
// Response: Add Header
//------------------------------------------------------------------
void HttpResponse::add_header(std::string name, std::string value) {
  headers.push_back(name + ": " + value + "\r\n");
}

//------------------------------------------------------------------
// Response: Write data to response
//------------------------------------------------------------------
void HttpResponse::write(const std::string& msg) {
  body.push_back(msg);
}

//------------------------------------------------------------------
// Response      = Status-Line               ; Section 6.1
//                 *(( general-header        ; Section 4.5
//                  | response-header        ; Section 6.2
//                  | entity-header ) CRLF)  ; Section 7.1
//                 CRLF
//                 [ message-body ]          ; Section 7.2
//
// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
//------------------------------------------------------------------
HttpResponse::operator std::string() {
  std::string full_result =
      "HTTP/1.0 " + std::to_string(status_code) + " " + reason_phrase + "\r\n";

  for (auto& header : headers) {
    full_result += header;
  }
  full_result += "\r\n";

  full_result += utils::concat_string(body);

  return full_result;
}

//------------------------------------------------------------------
// Test
//------------------------------------------------------------------
void subtest() {
  char test_result[] =
      "HTTP/1.0 404 Not found\r\n"
      "\r\n";
  http::HttpResponse res(404, "Not found");
  assert(memcmp(((std::string)res).c_str(), test_result, sizeof(test_result)) == 0);

  char test_result2[] =
      "HTTP/1.0 404 Not found\r\n"
      "\r\nMessage";
  http::HttpResponse res2(404, "Not found", "Message");
  assert(memcmp(((std::string)res2).c_str(), test_result2, sizeof(test_result2)) == 0);
}

void unit_test() {
  /// TESTs
  http::HttpParser parser;

  std::string data[] = {
      "GET "
      "/Protocols/rfc2616/"
      "rfc2616-sec5.html?test=value&param1=keys&param2=ee&empty=&test=true "
      "HTTP/1.1\r\n",
      "Host:www.w3.org\r\n", "Connection: keep-alive\r\n", "Pragma: no-cache\r\n",
      "Cache-Control: no-cache\r\n",
      "Accept: "
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/"
      "*;q=0.8\r\n",
      "Upgrade-Insecure-Requests: 1\r\n",
      "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_4) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/50.0.2661.102 "
      "Safari/537.36\r\n",
      "Referer: https://www.w3.org/Protocols/rfc2616/rfc2616.html\r\n",
      "Accept-Encoding: gzip, deflate, sdch\r\n", "Accept-Language: en-US,en;q=0.8,ru;q=0.6\r\n",
      "\r\n",
      "<!DOCTYPE html><p>\
       A request message from a client to a server includes, within the\
       first line of that message, the method to be applied to the resource,\
       the identifier of the resource, and the protocol version in use.\
    </p></html>"};

  int i = 0;
  int len = sizeof(data) / sizeof(data[0]);
  do {
    // std::cout << data[i];
    parser.write(data[i].c_str(), data[i].length());
  } while (++i < len);

  // std::cout << parser.headers["Pragma"] << std::endl;

  assert(parser.is_request_complete() == true);
  assert(parser.is_headers_complete() == true);
  assert(parser.headers["pragma"] == "no-cache");
  assert(parser.headers["host"] == "www.w3.org");
  assert(parser.request.method == "GET");
  assert(parser.request.uri ==
         "/Protocols/rfc2616/"
         "rfc2616-sec5.html?test=value&param1=keys&param2=ee&empty=&test=true");
  assert(parser.request.query.path == "/Protocols/rfc2616/rfc2616-sec5.html");
  assert(parser.request.query.params["test"] == "value");
  assert(parser.request.query.params["param1"] == "keys");
  assert(parser.request.query.params["empty"] == "");
  assert(parser.request.query.params["undefined"] == "");

  assert(parser.get_body() ==
         "<!DOCTYPE html><p>       A request message from a client to a server "
         "includes, within the       first line of that message, the method to "
         "be applied to the resource,       the identifier of the resource, "
         "and the protocol version in use.    </p></html>");

  assert(parser.get_headers() ==
         "GET "
         "/Protocols/rfc2616/"
         "rfc2616-sec5.html?test=value&param1=keys&param2=ee&empty=&test=true "
         "HTTP/1.1\r\n"
         "Host:www.w3.org\r\n"
         "Connection: keep-alive\r\n"
         "Pragma: no-cache\r\n"
         "Cache-Control: no-cache\r\n"
         "Accept: "
         "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/"
         "*;q=0.8\r\n"
         "Upgrade-Insecure-Requests: 1\r\n"
         "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_4) "
         "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/50.0.2661.102 "
         "Safari/537.36\r\n"
         "Referer: https://www.w3.org/Protocols/rfc2616/rfc2616.html\r\n"
         "Accept-Encoding: gzip, deflate, sdch\r\n"
         "Accept-Language: en-US,en;q=0.8,ru;q=0.6\r\n"
         "\r\n");

  // RESPONSE TEST
  subtest();

  char test_result[] =
      "HTTP/1.0 200 OK\r\n"
      "Accept: */*\r\n"
      "Authorization: basic\r\n"
      "\r\n"
      "test message!";

  char test[] = "test ";
  char message[] = "message!";

  http::HttpResponse res(200, "OK");
  res.add_header("Accept", "*/*");
  res.add_header("Authorization", "basic");
  res.write(test);
  res.write(message);

  assert(memcmp(((std::string)res).c_str(), test_result, sizeof(test_result)) == 0);

  //-------------------------------------------
  // POST check

  uint8_t post_data[] =
      "POST "
      "/method/test?"
      "test=value&param1=keys&param2=ee&empty=&test=true "
      "HTTP/1.1\r\n"
      "Host: www.w3.org\r\n"
      "Connection: keep-alive\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "Accept: "
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/"
      "*;q=0.8\r\n"
      "Content-Length: 21\r\n"
      "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_4) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/50.0.2661.102 "
      "Safari/537.36\r\n"
      "Referer: https://www.w3.org/Protocols/rfc2616/rfc2616.html\r\n"
      "Accept-Encoding: gzip, deflate, sdch\r\n"
      "Accept-Language: en-US,en;q=0.8,ru;q=0.6\r\n"
      "\r\n"
      "1234567890\000"
      "abcdefghik";

  http::HttpParser parser2;

  int l = 0;
  while (parser2.is_request_complete() != true) {
    if (l >= 544) {
      assert(parser2.is_headers_complete() == true);
      assert(parser2.is_request_complete() == false);
    }

    // std::cout << "L:" << l << " msg:" << post_data[l] << std::endl;
    parser2.write((const char*)post_data + l, 1);
    l++;
  }

  assert(parser2.headers["content-length"] == "21");
  assert(memcmp(parser2.get_body().c_str(), "1234567890\000abcdefghik", 22) == 0);

  std::cout << "OK =)" << std::endl;
}
}