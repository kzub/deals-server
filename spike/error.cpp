#include "iostream"

enum class ErrorCode { RequestError, InternalError };

class Error {
 public:
  Error(std::string _text, ErrorCode _code = ErrorCode::RequestError)
      : message(_text), code(getErrorCode(_code)), type(getErrorType(_code)) {
  }

  const std::string type;
  const std::string message;
  const uint16_t code;

 private:
  static uint16_t getErrorCode(ErrorCode);
  static std::string getErrorType(ErrorCode);
};

std::string Error::getErrorType(ErrorCode code) {
  switch (code) {
    case ErrorCode::RequestError:
      return "Bad request";
    default:
      return "-";
  }
};
uint16_t Error::getErrorCode(ErrorCode code) {
  switch (code) {
    case ErrorCode::RequestError:
      return 400;
    default:
      return 0;
  }
};

int main() {
  Error err("date_time", ErrorCode::RequestError);
  std::cout << "OK:" << err.type << std::to_string(err.code << err.message << std::endl;
  return 0;
}