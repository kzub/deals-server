#include <algorithm>
#include <cinttypes>

#include "utils.hpp"

namespace utils {
/*------------------------------------------------------------------
* Params container and accessor
------------------------------------------------------------------*/
std::string ObjectMap::operator[](std::string name) {
  return findValueInObjs(mapStorage, name);
}

void ObjectMap::add_object(Object obj) { mapStorage.push_back(obj); }

/*-----------------------------------------------------
  split strings by delimiter and put it into vector
-----------------------------------------------------*/
std::vector<std::string> split_string(std::string text, std::string delimiter) {
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
    // std::cout << *msg << " " << msg->size() << std::endl;
    concated_msg += *msg;
  }
  return concated_msg;
}

/*------------------------------------------------------------------
* util: lowercase
------------------------------------------------------------------*/
std::string toLowerCase(std::string text) {
  // transform lowercase
  std::transform(text.begin(), text.end(), text.begin(), ::tolower);
  return text;
}

std::string toUpperCase(std::string text) {
  // transform lowercase
  std::transform(text.begin(), text.end(), text.begin(), ::toupper);
  return text;
}

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
}