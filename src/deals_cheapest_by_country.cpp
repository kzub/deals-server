#include "deals_cheapest_by_country.hpp"

namespace deals {
//---------------------------------------------------------
void CheapestByCountry::pre_search() {
  reset_group_max_price();
  if (filter_destination_country) {
    filter_result_limit = destination_country_set.size();
  }
}

//---------------------------------------------------------
void CheapestByCountry::process_deal(const i::DealInfo &deal) {
  if (grouped_by_country.size() >= filter_result_limit) {
    if (more_than_group_max_price(deal.price)) {
      return;
    }
  }

  auto &dst_deal = grouped_by_country[deal.destination_country];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    dst_deal = deal;
    update_group_max_price(deal.price);
  }
  // if  not cheaper but same  destination & dates, replace with newer results
  else if (deal.destination == dst_deal.destination &&
           deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.direct == dst_deal.direct) {
    dst_deal = deal;
    update_group_max_price(deal.price);
    dst_deal.overriden = true;  // it is used in tests
  }
}

//----------------------------------------------------------------
void CheapestByCountry::post_search() {
  for (const auto &deal : grouped_by_country) {
    exec_result.push_back(deal.second);
  }

  // sort by departure_date ASC
  std::sort(exec_result.begin(), exec_result.end(),
            [](const i::DealInfo &a, const i::DealInfo &b) { return a.price < b.price; });
}

//----------------------------------------------------------------
const std::vector<i::DealInfo> CheapestByCountry::get_result() const {
  return exec_result;
}
}  // namespace deals