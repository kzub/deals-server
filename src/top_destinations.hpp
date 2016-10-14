#ifndef SRC_TOP_DST_HPP
#define SRC_TOP_DST_HPP

#include <unordered_map>
#include "cache.hpp"
#include "search_query.hpp"
#include "shared_memory.hpp"

namespace top {

#define TOPDST_EXPIRES DEALS_EXPIRES

#define TOPDST_TABLENAME "TopDst"
#define TOPDST_PAGES 5000
#define TOPDST_ELEMENTS 10000

void unit_test();

namespace i {
struct DstInfo {
  uint16_t locale;
  uint32_t destination;
  uint32_t departure_date;
};
}  // namespace i

struct DstInfo {
  uint32_t destination;
  uint32_t counter;
};

namespace utils {
void print(const i::DstInfo& deal);
void print(const DstInfo& deal);
}  // namespace utils

//-----------------------------------------------------------
// TopDstDatabase
//-----------------------------------------------------------
class TopDstDatabase {
 public:
  TopDstDatabase();
  ~TopDstDatabase();

  bool addDestination(std::string locale, std::string destination, std::string departure_date);
  std::vector<DstInfo> getLocaleTop(std::string locale, std::string departure_date_from,
                                    std::string departure_date_to, uint16_t limit);

  std::vector<DstInfo> getCachedResult(std::string locale, std::string departure_date_from,
                                       std::string departure_date_to, uint16_t limit);

  void saveResultToCache(std::string locale, std::vector<DstInfo>& result);
  void truncate();  // clear database
 private:
  using CachedResult = cache::Cache<std::vector<DstInfo>>;
  shared_mem::Table<i::DstInfo>* db_index;
  std::unordered_map<std::string, CachedResult> result_cache_by_locale;

  friend void unit_test();
};

//-----------------------------------------------------
// TopDstSearchQuery
//-----------------------------------------------------
class TopDstSearchQuery : public shared_mem::TableProcessor<i::DstInfo>, public query::SearchQuery {
 public:
  TopDstSearchQuery(shared_mem::Table<i::DstInfo>& table) : table(table) {
  }

 protected:
  std::vector<DstInfo> exec();

  // function that will be called by TableProcessor
  // for iterating over all not expired pages in table
  void process_element(const i::DstInfo& element);

  friend class TopDstDatabase;

 private:
  shared_mem::Table<i::DstInfo>& table;
  std::unordered_map<uint32_t, uint32_t> grouped_destinations;
};
}  // namespace top

#endif