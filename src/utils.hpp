#ifndef SRC_UTILS_HPP
#define SRC_UTILS_HPP

#include <iostream>
#include <vector>

namespace utils {
/*-----------------------------------------------------
  key value storage for internal use
-----------------------------------------------------*/
struct Object {
  std::string name;
  std::string value;
};

/*------------------------------------------------------------------
* Key-Value container and accessor
------------------------------------------------------------------*/
class ObjectMap {
 public:
  // params accessor
  std::string operator[](std::string name);
  void add_object(Object obj);

 private:
  std::vector<Object> mapStorage;
};

/*-----------------------------------------------------
  split strings by delimiter and put it into vector
-----------------------------------------------------*/
std::vector<std::string> split_string(std::string text,
                                      std::string delimiter = ",");

/*------------------------------------------------------------------
* util: concat string
------------------------------------------------------------------*/
std::string concat_string(std::vector<std::string> msgs);

/*-----------------------------------------------------
  utils: search by key in object storage
-----------------------------------------------------*/
std::string findValueInObjs(std::vector<Object> objs, std::string name);

/*-----------------------------------------------------
  utils: search by key in object storage
-----------------------------------------------------*/
std::string toLowerCase(std::string);
std::string toUpperCase(std::string);
}
#endif