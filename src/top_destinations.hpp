#ifndef SRC_TOP_DST_HPP
#define SRC_TOP_DST_HPP

#include <unordered_map>
#include "cache.hpp"
#include "search_query.hpp"
#include "shared_memory.hpp"
#include "types.hpp"

namespace top {

#define TOPDST_EXPIRES DEALS_EXPIRES

#define TOPDST_TABLENAME "TopDst"
#define TOPDST_PAGES 5000
#define TOPDST_ELEMENTS 10000

void unit_test();

namespace i {
struct DstInfo {
  uint8_t locale;
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

  void addDestination(const types::CountryCode& locale, const types::IATACode& destination,
                      const types::Date& departure_date);

  std::vector<DstInfo> getLocaleTop(const types::Required<types::CountryCode>& locale,
                                    const types::Optional<types::Date>& departure_date_from,
                                    const types::Optional<types::Date>& departure_date_to,
                                    const types::Optional<types::Number>& limit);

  std::vector<DstInfo> getCachedResult(const types::CountryCode& locale,
                                       const types::Date& departure_date_from,
                                       const types::Date& departure_date_to,
                                       const types::Number& limit);

  void saveResultToCache(const types::CountryCode& locale, const types::Date& departure_date_from,
                         const types::Date& departure_date_to, const std::vector<DstInfo>& result);

  void truncate();  // clear database
 private:
  shared_mem::Table<i::DstInfo>* db_index;

  using CachedResult = const cache::Cache<std::vector<DstInfo>>;
  std::unordered_map<uint8_t, CachedResult> result_cache_by_locale;

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

 private:
  shared_mem::Table<i::DstInfo>& table;
  std::unordered_map<uint32_t, uint32_t> grouped_destinations;

  friend class TopDstDatabase;
};
}  // namespace top

#endif