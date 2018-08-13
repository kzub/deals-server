#include "deals_types.hpp"
//***********************************************************
//                   UTILS
//***********************************************************
namespace deals {
namespace utils {
//-----------------------------------------------------------
void print(const i::DealInfo& deal) {
  std::cout << "i::DEAL: (" << types::int_to_date(deal.departure_date) << ")"
            << types::code_to_origin(deal.origin) << "-" << types::code_to_origin(deal.destination)
            << (deal.return_date ? ("(" + types::int_to_date(deal.return_date) + ") :")
                                 : "             :")
            << deal.price << " " << deal.page_name << ":" << deal.index << std::endl;
}
//-----------------------------------------------------------
void print(const DealInfo& deal) {
  if (deal.test == nullptr) {
    // src/deals_database.hpp (TEST_BUILD = 1)
    std::cerr << "print() error: No test data. Rebuild code with TEST_BUILD enabled" << std::endl;
    return;
  }

  std::cout << "DEAL: (" << deal.test->departure_date << ")" << deal.test->origin << "-"
            << deal.test->destination << "(" << deal.test->return_date << ")"
            << (deal.test->overriden ? "w" : " ") << ": " << deal.test->price << std::endl;
}
//-----------------------------------------------------------
std::string sprint(const DealInfo& deal) {
  if (deal.test == nullptr) {
    // src/deals_database.hpp (TEST_BUILD = 1)
    std::cerr << "print() error: No test data. Rebuild code with TEST_BUILD enabled" << std::endl;
    return "no test data\n";
  }

  return "(" + deal.test->departure_date + ")" + deal.test->origin + "-" + deal.test->destination +
         "(" + deal.test->return_date + ") : " + std::to_string(deal.test->price) + "|" +
         deal.data + "\n";
}
//-----------------------------------------------------------
bool equal(const i::DealInfo& d1, const i::DealInfo& d2) {
  return (d1.departure_date == d2.departure_date) && (d1.return_date == d2.return_date) &&
         (d1.direct == d2.direct) && (d1.destination == d2.destination) && (d1.origin == d2.origin);
}

// example:
// date  17  14  17
// price $14 $15 $16
// ----- time ------>
// $16 override $14 as newer, but $15 must be selected as cheapest inline
//----------------------------------------------------------------
// check for result is not cheapest and outdated at the same time
//----------------------------------------------------------------
const i::DealInfo findCheapestAndLast(const std::vector<i::DealInfo>& history) {
  auto len = history.size();
  if (len == 0) {
    return {};
  }
  auto last_deal = history[len - 1];
  len--;

  for (auto i = 0; i < len; i++) {
    const auto& d = history[i];
    // when result contain equivalent deals with different timestamps
    if (utils::equal(d, last_deal)) {
      // if cheapest is outdated => select previous deal from history
      // and start from the begining
      if (d.timestamp > last_deal.timestamp) {
        len--;
        last_deal = history[len];
        i = 0;
        continue;
      }
    }
  }

  // when we rich this point last_deal - is cheapest and newest available
  return last_deal;
}
//-------------------------------------------------------------
}  // utils namespace
}  // namespace deals
