#include "deals_cheapest_by_date.hpp"

namespace deals {
//----------------------------------------------------------------
void CheapestByDay::pre_search() {
  if (!filter_departure_date || !departure_date_values.duration) {
    std::cerr << "ERROR no departure_date range" << std::endl;
    throw types::Error("departure dates interval must be specified\n");
  }

  if (departure_date_values.duration > 366) {
    std::cerr << "ERROR departure_date_values.duration > 366:"
              << std::to_string(departure_date_values.duration) << std::endl;
    throw types::Error("Date interval to large. 365 days is maximum\n");
  }

  filter_result_limit = departure_date_values.duration;

  if (departure_date_values.duration == 1 && filter_return_date) {
    if (!return_date_values.duration) {
      std::cerr << "ERROR no return_date range" << std::endl;
      throw types::Error("return dates interval must be specified\n");
    }
    group_by_return_date = true;
    filter_result_limit = return_date_values.duration;
  }
}

//---------------------------------------------------------
void CheapestByDay::process_deal(const i::DealInfo &deal) {
  const auto date = group_by_return_date ? deal.return_date : deal.departure_date;
  auto &dst_deal = grouped_by_date[date];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    if (dst_deal.price == deal.price && dst_deal.timestamp > deal.timestamp) {
      return;
    }
    dst_deal = deal;
  }
  // if  not cheaper but same destination & dates, replace with newer results
  else if (deal.destination == dst_deal.destination && deal.return_date == dst_deal.return_date &&
           deal.direct == dst_deal.direct && dst_deal.timestamp < deal.timestamp) {
    dst_deal = deal;
    dst_deal.overriden = true;  // it is used in tests
  }
}

//----------------------------------------------------------------
void CheapestByDay::post_search() {
  for (const auto &deal : grouped_by_date) {
    exec_result.push_back(deal.second);
  }

  if (group_by_return_date) {
    std::sort(
        exec_result.begin(), exec_result.end(),
        [](const i::DealInfo &a, const i::DealInfo &b) { return a.return_date < b.return_date; });
  } else {
    std::sort(exec_result.begin(), exec_result.end(),
              [](const i::DealInfo &a, const i::DealInfo &b) {
                return a.departure_date < b.departure_date;
              });
  }
}

//----------------------------------------------------------------
const std::vector<i::DealInfo> CheapestByDay::get_result() const {
  return exec_result;
}
}  // namespace deals