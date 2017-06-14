#include "deals_unique_routes.hpp"

namespace deals {
//------------------------------------------------------------
// UniqueRoutes
//------------------------------------------------------------
const std::vector<DealInfo> getUniqueRoutesRoutine(shared_mem::Table<i::DealInfo>& table) {
  UniqueProcessor up;
  table.processRecords(up);
  return up.getResults();
}

void UniqueProcessor::process_element(const i::DealInfo& deal) {
  uint64_t route = ((uint64_t)deal.origin << 32) + deal.destination;
  auto& dst_deal = grouped_by_routes[route];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    if (dst_deal.price == deal.price && dst_deal.timestamp > deal.timestamp) {
      return;
    }
    dst_deal = deal;
  }
  // if  not cheaper but same dates and direct/stops, replace with newer results
  else if (deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.direct == dst_deal.direct &&
           dst_deal.timestamp < deal.timestamp) {
    dst_deal = deal;
    dst_deal.overriden = true;
  }
}

std::vector<DealInfo> UniqueProcessor::getResults() {
  // std::vector<DealInfo> exec_result;
  // for (const auto& deal : grouped_by_routes) {
  //   utils::print(deal.second);
  // }
  std::cout << "Total unique routes:" << grouped_by_routes.size() << std::endl;

  return {};
}

}  // namespace deals
