#include "deals_stats.hpp"
#include "types.hpp"

namespace deals {
//------------------------------------------------------------
// UniqueRoutes
//------------------------------------------------------------
const std::string getStatsRoutine(shared_mem::Table<i::DealInfo>& table) {
  StatsProcessor stat;
  table.processRecords(stat);
  return stat.getStringResults();
}

void StatsProcessor::process_element(const i::DealInfo& deal) {
  elements++;
  size += deal.size;

  if (deal.timestamp > max) {
    max = deal.timestamp;
  }

  if (deal.timestamp < min) {
    min = deal.timestamp;
  }

  /*
  std::string route = types::int_to_date(deal.departure_date) + types::code_to_origin(deal.origin) +
                      types::code_to_origin(deal.destination);

  if (deal.return_date) {
    route = route + types::int_to_date(deal.return_date) + ",RT";
  } else {
    route = route + ",OW";
  }

  group_by_route[route]++;
  */
}

const std::string StatsProcessor::getStringResults() {
  std::string res;

  /*
  for (const auto& elm : group_by_route) {
    std::cout << elm.first << "," << std::to_string(elm.second) << std::endl;
  }
  */

  res = "{\"elements\":" + std::to_string(elements) + ",\"size\":" + std::to_string(size) +
        ",\"min\":" + std::to_string(min) + ",\"max\":" + std::to_string(max) + "}";

  std::cout << "result: " << res << std::endl;
  return res;
}

}  // namespace deals
