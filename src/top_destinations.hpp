#ifndef SRC_TOP_DST_HPP
#define SRC_TOP_DST_HPP

#include "deals.hpp"
#include "search_query.hpp"
#include "shared_memory.hpp"
#include "utils.hpp"

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
}

struct DstInfo {
  uint32_t destination;
  uint32_t counter;
};

namespace utils {
void copy(i::DstInfo& dst, const i::DstInfo& src);
void print(const i::DstInfo& deal);
void print(const DstInfo& deal);
};

class TopDstDatabase {
 public:
  TopDstDatabase();
  ~TopDstDatabase();

  bool addDestination(std::string locale, std::string destination, std::string departure_date);
  std::vector<DstInfo> getLocaleTop(std::string locale, std::string departure_date_from,
                                    std::string departure_date_to, uint16_t limit);

  void truncate();

 private:
  shared_mem::Table<i::DstInfo>* db_index;

  friend void unit_test();
};

class TopDstSearchQuery : public shared_mem::TableProcessor<i::DstInfo>, public query::SearchQuery {
 public:
  TopDstSearchQuery(shared_mem::Table<i::DstInfo>& table) : table(table) {
  }

 protected:
  std::vector<DstInfo> exec();

  /* function that will be called by TableProcessor
        *  for iterating over all not expired pages in table */
  bool process_function(i::DstInfo* elements, uint32_t size);

  friend class TopDstDatabase;

 private:
  shared_mem::Table<i::DstInfo>& table;
  std::vector<DstInfo> top_destinations;
};
}

#endif