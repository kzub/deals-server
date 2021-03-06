#include "deals_cheapest_by_country.hpp"

namespace deals {
//---------------------------------------------------------
void CheapestByCountry::pre_search() {
  if (filter_destination_country) {
    filter_result_limit = destination_country_set.size();
  }

  if (filter_all_combinations) {
    std::cerr << "ERROR all_combinations not implemented for by_country aggregation" << std::endl;
    throw types::Error("all_combinations flag not implemented for by_country aggregation\n");
  }
}

//---------------------------------------------------------
void CheapestByCountry::process_deal(const i::DealInfo &deal) {
  auto &dst_deal = grouped_by_country[deal.destination_country];

  if (dst_deal.price == 0 || deal.price <= dst_deal.price) {
    // ignore same route and dates with lower price, but older timestamp
    if (utils::equal(deal, dst_deal) && dst_deal.timestamp > deal.timestamp) {
      return;
    }
    dst_deal = deal;
    grouped_by_country_hist[deal.destination_country].push_back(deal);
    return;
  }

  // if  not cheaper but same  destination & dates, replace with newer results
  if (utils::equal(deal, dst_deal) && dst_deal.timestamp < deal.timestamp) {
    dst_deal = deal;
    grouped_by_country_hist[deal.destination_country].push_back(deal);
    dst_deal.overriden = true;  // it is used in tests
  }
}

//----------------------------------------------------------------
void CheapestByCountry::post_search() {
  for (const auto &deal : grouped_by_country_hist) {
    exec_result.push_back(utils::findCheapestAndLast(deal.second));
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