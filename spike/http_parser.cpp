#include <cassert>
#include <cinttypes>
#include <iostream>
#include <vector>

namespace http {

/*-----------------------------------------------------
  split strings by delimiter and put it into vector
-----------------------------------------------------*/
std::vector<std::string> split_string(std::string text,
                                      std::string delimiter = ",") {
  std::vector<std::string> result;

  while (text.length()) {
    size_t pos = text.find(delimiter);

    if (pos == -1) {
      result.push_back(text);
      return result;
    }

    std::string token = text.substr(0, pos);
    result.push_back(token);
    text = text.substr(pos + delimiter.length(), std::string::npos);
  }

  return result;
}

/*------------------------------------------------------------------
* util: concat string
------------------------------------------------------------------*/
std::string concat_string(std::vector<std::string> msgs) {
  std::string concated_msg;
  for (std::vector<std::string>::iterator msg = msgs.begin(); msg != msgs.end();
       ++msg) {
    concated_msg += *msg;
  }
  return concated_msg;
}

/*-----------------------------------------------------
  key value storage for internal use
-----------------------------------------------------*/
struct Object {
  std::string name;
  std::string value;
};

/*-----------------------------------------------------
  utils: search by key in object storage
-----------------------------------------------------*/
std::string findValueInObjs(std::vector<Object> objs, std::string name) {
  for (std::vector<Object>::iterator obj = objs.begin(); obj != objs.end();
       ++obj) {
    if (obj->name == name) {
      return obj->value;
    }
  }
  std::string empty;
  return empty;
}

// ------------------------------------------------------------------

class HttpHeaders {
 public:
  // parse incoming text and if double CRLF found
  // than process and save headers
  uint8_t parse(std::string http_message) {
    if (!http_message.length()) {
      return -1;
    }

    size_t pos = http_message.find("\r\n\r\n");
    if (pos == -1) {
      return -1;
    }

    // headers are fully loaded. let we parse it
    std::string http_headers = http_message.substr(0, pos);
    std::vector<std::string> http_headers_lines =
        split_string(http_headers, "\r\n");

    // std::cout << "RequestLine:" << http_headers_lines[0] << std::endl;

    // omit first one as it is a request line
    for (std::vector<std::string>::iterator line =
             http_headers_lines.begin() + 1;
         line != http_headers_lines.end(); ++line) {
      // std::cout << "HeaderLine:" << *line << std::endl;
      // split for name & value
      size_t hpos = line->find(":");
      if (hpos == -1) {
        // unknown or bad header format
        return -1;
      }

      Object one_header;
      one_header.name = line->substr(0, hpos);

      // remove space if it exists after ':'
      if ((*line)[hpos + 1] == ' ') {
        hpos++;
      }
      one_header.value = line->substr(hpos + 1);

      headers_obj.push_back(one_header);
    }

    return 0;
  }

  // define accessor
  std::string operator[](std::string name) {
    return findValueInObjs(headers_obj, name);
  }

 private:
  std::vector<Object> headers_obj;
};

/*------------------------------------------------------------------
* Params container and accessor
------------------------------------------------------------------*/
class Params {
 public:
  // params accessor
  std::string operator[](std::string name) {
    return findValueInObjs(params_obj, name);
  }

 protected:
  std::vector<Object> params_obj;
  Params(){};
  friend class URIQueryParams;
};

/*------------------------------------------------------------------
* Query params parser and storage
------------------------------------------------------------------*/
class URIQueryParams {
 public:
  void parse(std::string query_text) {
    if (!query_text.length()) {
      return;
    }

    size_t pos = query_text.find("?");
    if (pos == -1) {
      path = query_text;
      return;
    }

    path = query_text.substr(0, pos);

    // separate params strings by '&' char
    std::vector<std::string> query_params =
        split_string(query_text.substr(pos + 1), "&");

    // for every param 'param1=value' make an object
    for (std::vector<std::string>::iterator param = query_params.begin();
         param != query_params.end(); ++param) {
      Object one_param;
      // std::cout << "param;" << *param << std::endl;
      size_t pos = param->find("=");
      if (pos == -1) {
        one_param.name = *param;
      } else {
        one_param.name = param->substr(0, pos);
        one_param.value = param->substr(pos + 1);
      }

      params.params_obj.push_back(one_param);
    }
  };

  Params params;
  std::string path;
};

/*------------------------------------------------------------------
* Request parser and storage
------------------------------------------------------------------*/
class HttpRequest {
 public:
  void parse(std::string requestline) {
    if (!requestline.length()) {
      return;
    }

    size_t pos = requestline.find("\r\n");
    if (pos == -1) {
      return;
    }

    /* Request-Line   = Method SP Request-URI SP HTTP-Version CRLF */
    std::vector<std::string> res =
        split_string(requestline.substr(0, pos), " ");
    if (res.size() != 3) {
      return;
    }

    method = res[0];
    uri = res[1];
    http_version = res[2];

    query.parse(uri);
  }

  std::string method;
  std::string uri;
  std::string http_version;

  URIQueryParams query;
};

// HTTP REQUEST FORMAT:
// Request       = Request-Line              ; Section 5.1
//                 *(( general-header        ; Section 4.5
//                  | request-header         ; Section 5.3
//                  | entity-header ) CRLF)  ; Section 7.1
//                 CRLF
//                 [ message-body ]          ; Section 4.3
// Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
// ------------------------------------------------------------------
class HttpParser {
 public:
  void write(std::string msg) {
    msgs.push_back(msg);

    if (!headers_written) {
      splitHeaders();
    }
  }

  HttpParser() : headers_written(false), headers_end(0) {}

  HttpHeaders headers;
  HttpRequest request;
  std::string get_body() { return concat_string(msgs).substr(headers_end); }

  std::string get_headers() {
    return concat_string(msgs).substr(0, headers_end);
  }

 private:
  std::vector<std::string> msgs;
  bool headers_written;
  size_t headers_end;

  void splitHeaders() {
    std::string concated_msg = concat_string(msgs);

    // check if header are fully downloaded
    size_t pos = concated_msg.find("\r\n\r\n");
    if (pos == -1) {
      return;
    }

    if (headers.parse(concated_msg) == 0) {
      headers_end = pos + 4;
      headers_written = true;
      request.parse(concated_msg);
    } else {
      std::cout << "error parsing HTTP headers";
    }
  }
};
}

// ------------------------------------------------------------------

int main() {
  /// TESTs
  http::HttpParser parser;

  std::string data[] = {
      "GET "
      "/Protocols/rfc2616/"
      "rfc2616-sec5.html?test=value&param1=keys&param2=ee&empty=&test=true "
      "HTTP/1.1\r\n",
      "Host: www.w3.org\r\n", "Connection: keep-alive\r\n",
      "Pragma: no-cache\r\n", "Cache-Control: no-cache\r\n",
      "Accept: "
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/"
      "*;q=0.8\r\n",
      "Upgrade-Insecure-Requests: 1\r\n",
      "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_4) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/50.0.2661.102 "
      "Safari/537.36\r\n",
      "Referer: https://www.w3.org/Protocols/rfc2616/rfc2616.html\r\n",
      "Accept-Encoding: gzip, deflate, sdch\r\n",
      "Accept-Language: en-US,en;q=0.8,ru;q=0.6\r\n", "\r\n",
      "<!DOCTYPE html><p>\
       A request message from a client to a server includes, within the\
       first line of that message, the method to be applied to the resource,\
       the identifier of the resource, and the protocol version in use.\
    </p></html>"};

  int i = 0;
  int len = sizeof(data) / sizeof(data[0]);
  do {
    // std::cout << data[i];
    parser.write(data[i]);
  } while (++i < len);

  assert(parser.headers["Pragma"] == "no-cache");
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
         "Host: www.w3.org\r\n"
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
  return 0;
}