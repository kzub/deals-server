#include <initializer_list>
#include <iostream>
#include <map>
#include <utility>

class Tags : public std::map<const std::string, std::string> {
  using KeyValue = const std::pair<const std::string, const std::string>;

 public:
  Tags(const std::initializer_list<KeyValue> &&kv) {
    for (const auto &val : kv) {
      this->insert(val);
    }
  };
};

// class Tags : public std::map<std::string, std::string> {
//  public:
//   Tags(std::map<std::string, std::string> &&map)
//       : std::map<std::string, std::string>(map){

//         };
// };

// using Tags = std::map<std::string, std::string>;
#define makeType(Name, T) \
  struct Name {           \
   public:                \
    T v;                  \
  };

#define VAR(Name) Name.v

makeType(Flight, int);

int main(int argc, char const *argv[]) {
  Flight fv;
  VAR(fv) = 10;

  std::cout << std::to_string(VAR(fv)) << std::endl;

  Tags tags{{"key", "value"}, {"key2", "value2"}, {"key3", "value3"}};
  // std::map<std::string, std::string>{{"key", "value"}, {"key", "value"}, {"key2", "value2"}};

  tags["dd"] = "tt";
  tags["aa"] = "len";

  for (const auto &t : tags) {
    std::cout << t.first << ": " << t.second << std::endl;
  }
  return 0;
}