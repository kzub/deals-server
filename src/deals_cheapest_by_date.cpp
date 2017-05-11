#include "deals_cheapest_by_date.hpp"

namespace deals {
//----------------------------------------------------------------
void CheapestByDay::pre_search() {
  checkInputParams();

  if (filter_exact_date) {
    if (departure_return_max_duration) {
      filter_result_limit = departure_return_max_duration;
    }
    return;
  }

  if (departure_date_values.duration <= 1) {
    group_by_return_date = true;
    if (return_date_values.duration) {
      filter_result_limit = return_date_values.duration;
    }
    return;
  }

  if (departure_date_values.duration > 1) {
    filter_result_limit = departure_date_values.duration;
  } else {
    filter_result_limit = return_date_values.duration;
  }
}

void CheapestByDay::checkInputParams() {
  if (!departure_date_values.duration && !return_date_values.duration) {
    std::cerr << "ERROR no departure or return dates interval" << std::endl;
    throw types::Error("Departure or return dates interval must be specified\n");
  }
  if (departure_date_values.duration > 366 || return_date_values.duration > 366) {
    std::cerr << "ERROR departure or return dates interval > 365."
              << " Dep:" << std::to_string(departure_date_values.duration)
              << " Ret:" << std::to_string(return_date_values.duration) << std::endl;
    throw types::Error("Date interval to large. 365 days is maximum\n");
  }
}

//---------------------------------------------------------
query::DateValue CheapestByDay::getDateToGroup(const i::DealInfo &deal) {
  if (filter_exact_date) {
    if (exact_date_value == deal.return_date) {
      return deal.departure_date;
    }

    // exact_date_value == deal.departure_date
    // set first bit to 1 for group return dates separately
    return deal.return_date | 0x80000000;
  }

  if (group_by_return_date) {
    return deal.return_date;
  }

  return deal.departure_date;
}

//---------------------------------------------------------
void CheapestByDay::process_deal(const i::DealInfo &deal) {
  //
  auto &dst_deal = grouped_by_date[getDateToGroup(deal)];

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
  const auto exact_date = exact_date_value;

  if (filter_exact_date) {
    std::sort(exec_result.begin(), exec_result.end(),
              [&exact_date](const i::DealInfo &a, const i::DealInfo &b) {
                if (exact_date == a.return_date && exact_date == b.return_date) {
                  return a.departure_date < b.departure_date;
                }
                if (exact_date == a.departure_date && exact_date == b.departure_date) {
                  return a.return_date < b.return_date;
                }
                if (exact_date == a.return_date) {
                  return true;
                }
                return false;
              });
  } else if (group_by_return_date) {
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