#include <iostream>
#include <map>
#include <vector>

std::string json_text =
    R"({"object": {"int": 1, "string": "hellow world"}, "num": 2, "str": "hi"})";

enum class ObjectTypes { Undefined, Number, String, Map };

class Object {
 public:
  Object(std::string name) : name{name} {};

  virtual int getInt() = 0;
  virtual std::string getString() = 0;

  // protected:
  std::string name;
  ObjectTypes type = ObjectTypes::Undefined;
};

class Undefined : public Object {
 public:
  Undefined() : Object{"undefined"} {
    type = ObjectTypes::Undefined;
  }

  int getInt() override {
    return 0;
  };
  std::string getString() override {
    return "undefined";
  };
};

class Number : public Object {
 public:
  Number(std::string name, int value) : Object{name}, value{value} {
    type = ObjectTypes::Number;
  }

  // protected:
  int value = 0;
};

class String : public Object {
 public:
  String(std::string name, std::string value) : Object{name}, value{value} {
    type = ObjectTypes::String;
  }

  // protected:
  std::string value = "";
};

class Map : public Object {
 public:
  Map(std::string name) : Object{name} {
    type = ObjectTypes::Map;
  }

  Object operator[](std::string key) {
    if (value.find(key) == value.end()) {
      return Undefined();
    }

    return *value[key];
  }

  void add(Object&& obj) {
    value[obj.name] = &obj;
  }

  void print(std::string spaces = "") {
    for (auto k : value) {
      std::cout << spaces << k.first << ": ";
      switch (k.second->type) {
        case ObjectTypes::Number:
          std::cout << static_cast<Number*>(k.second)->value;
          break;
        case ObjectTypes::String:
          std::cout << static_cast<String*>(k.second)->value;
          break;
        case ObjectTypes::Map:
          std::cout << std::endl;
          static_cast<Map*>(k.second)->print(spaces + " ");
          break;
        default:
          std::cout << "undefined";
      }
      std::cout << std::endl;
      // std::cout << (int)k.second->value;
    }
  }

  // protected:
  std::map<std::string, Object*> value;
};

class JSON {
 public:
  JSON(std::string json) {
    // {"object": {"int": 1, "string": "hellow world"}, "num": 2, "str": "hi"}
    int open_curl = 0;
    int kcount = 0;
    int kpos = 0;
    int pos = 0;
    auto map = new Map("");

    for (auto& j : json) {
      pos++;
      if (j == '{') {
        open_curl = 1;
        continue;
      }
      if (j == '}') {
        open_curl = 0;
        continue;
      }
      if (j == '"') {
        kcount++;
        if (kcount == 2) {
          kcount = 0;
          // std::cout << kpos << " " << pos << std::endl;
          std::cout << json.substr(kpos, pos - kpos - 1) << std::endl;
        }  // else {
        kpos = pos;
        //}
      }
    }
  }
};

int main() {
  Object k {
    "test",
  }
  // std::cout << json_text << std::endl;
  // auto obj = JSON(json_text);
  /*Map
          String
          String
          Map
                  String
                  Number

  Object:
          string
          type
  */
  /*
  auto map = Map("test");
  map.add(Number("k1", 10));
  map.add(Number("k2", 20));
  map.add(Number("k3", 30));
  map.add(String("k5", "hi man"));

  auto obj = Map("obj");
  obj.add(Number("a1", 10));
  obj.add(Number("a2", 20));
  obj.add(Number("a3", 30));
  obj.add(String("a5", "hi man"));

  map.add(std::move(obj));

  map.print();
*/
  return 0;
}