#include "deals_unique_routes.hpp"

namespace deals {
//------------------------------------------------------------
// UniqueRoutes
//------------------------------------------------------------
const std::string getUniqueRoutesRoutine(shared_mem::Table<i::DealInfo>& table) {
  UniqueProcessor up;
  table.processRecords(up);
  return up.getStringResults();
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

const std::string UniqueProcessor::getStringResults() {
  std::string res;

  for (const auto& elm : grouped_by_routes) {
    res += types::code_to_origin(elm.second.origin) + "," +
           types::code_to_origin(elm.second.destination) + "," + std::to_string(elm.second.price) +
           "\n";
  }
  std::cout << "Total unique routes:" << grouped_by_routes.size() << std::endl;

  return res;
}

}  // namespace deals
