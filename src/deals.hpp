#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include "shared_memory.hpp"
#include "utils.hpp"

namespace deals {

struct DealInfo {
  uint32_t timestamp;
  uint32_t origin;
  uint32_t destination;
  uint32_t departure_date;
  uint32_t return_date;
  uint8_t flags;  // direct?
  uint32_t price;
  char page_name[MEMPAGE_NAME_MAX_LEN];
  uint32_t index;
  uint32_t size;
};

struct Int32Interval {
  uint32_t from;
  uint32_t to;
};

typedef uint8_t DealData;

namespace utils {
union PlaceCodec {
  uint32_t int_code;
  char iata_code[4];
};

uint32_t origin_to_code(std::string code);
std::string code_to_origin(uint32_t code);
void copy(DealInfo& dst, const DealInfo& src);
uint16_t get_max_price_in_array(DealInfo*& dst, uint16_t size);
void print(const DealInfo& deal);
};

class DealsDatabase {
 public:
  DealsDatabase();
  ~DealsDatabase();

  void addDeal(std::string origin, std::string destination,
               uint32_t departure_date, uint32_t return_date,
               bool direct_flight, uint32_t price, DealData* data,
               uint32_t size);

  std::vector<DealInfo> searchForCheapestEver(std::string origin,
                                              std::string destinations);

 private:
  shared_mem::Table<DealInfo>* db_index;
  shared_mem::Table<DealData>* db_data;
};
}

#endif