#include "deals_stats.hpp"

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
}

const std::string StatsProcessor::getStringResults() {
  std::string res;

  res = "{\"elements\":" + std::to_string(elements) + ",\"size\":" + std::to_string(size) +
        ",\"min\":" + std::to_string(min) + ",\"max\":" + std::to_string(max) + "}";

  std::cout << "result: " << res << std::endl;
  return res;
}

}  // namespace deals
