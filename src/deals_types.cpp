#include "deals_types.hpp"
//***********************************************************
//                   UTILS
//***********************************************************
namespace deals {
namespace utils {
void print(const i::DealInfo& deal) {
  std::cout << "i::DEAL: (" << types::int_to_date(deal.departure_date) << ")"
            << types::code_to_origin(deal.origin) << "-" << types::code_to_origin(deal.destination)
            << (deal.return_date ? ("(" + types::int_to_date(deal.return_date) + ") :")
                                 : "             :")
            << deal.price << " " << deal.page_name << ":" << deal.index << std::endl;
}
void print(const DealInfo& deal) {
  if (deal.test == nullptr) {
    // src/deals_database.hpp (TEST_BUILD = 1)
    std::cerr << "print() error: No test data. Rebuild code with testing enabled" << std::endl;
    return;
  }

  std::cout << "DEAL: (" << deal.test->departure_date << ")" << deal.test->origin << "-"
            << deal.test->destination << "(" << deal.test->return_date << ")"
            << (deal.test->overriden ? "w" : " ") << ": " << deal.test->price << std::endl;
}

std::string sprint(const DealInfo& deal) {
  if (deal.test == nullptr) {
    // src/deals_database.hpp (TEST_BUILD = 1)
    std::cerr << "print() error: No test data. Rebuild code with testing enabled" << std::endl;
    return "no test data\n";
  }

  return "(" + deal.test->departure_date + ")" + deal.test->origin + "-" + deal.test->destination +
         "(" + deal.test->return_date + ") : " + std::to_string(deal.test->price) + "|" +
         deal.data + "\n";
}
}  // utils namespace
}  // namespace deals
