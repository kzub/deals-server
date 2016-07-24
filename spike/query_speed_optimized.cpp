//------------------------------------------------------------
// DealsCheapestByPeriod
//------------------------------------------------------------
class DealsCheapestByPeriod : public DealsSearchQuery {
 public:
  DealsCheapestByPeriod(shared_mem::Table<i::DealInfo> &table) : DealsSearchQuery{table} {
  }
  ~DealsCheapestByPeriod();

  // implement virtual functions:
  bool process_deal(const i::DealInfo &deal);
  void pre_search();
  void post_search();

  uint16_t deals_slots_used;
  uint16_t max_price_deal;

  std::vector<i::DealInfo> exec_result;
  i::DealInfo *result_deals = nullptr;  // size = filter_limit
  // this pointer used at search for speed optimization
  // for iterating throught simple values array but not vectors.
  // at exec() function there be local arrays this pointers
  // will point to
};

//------------------------------------------------------------
// DealsCheapestDayByDay
//------------------------------------------------------------
class DealsCheapestDayByDay : public DealsSearchQuery {
 public:
  DealsCheapestDayByDay(shared_mem::Table<i::DealInfo> &table) : DealsSearchQuery{table} {
  }
  ~DealsCheapestDayByDay();

  // implement virtual functions:
  bool process_deal(const i::DealInfo &deal);
  void pre_search();
  void post_search();

  // arrays of by date results:
  uint16_t deals_slots_used;
  uint16_t deals_slots_available;
  // uint16_t max_price_deal;

  // std::vector<std::vector<i::DealInfo>> exec_result;
  std::vector<i::DealInfo> exec_result;
  i::DealInfo *result_deals = nullptr;  // size = filter_limit
  // this pointer used at search for speed optimization
  // for iterating throught simple values array but not vectors.
  // at exec() function there be local arrays this pointers
  // will point to
};

//      ***************************************************
//                   CHEAPEST BY PERIOD
//      ***************************************************

/*---------------------------------------------------------
* DealsDatabase  searchForCheapestEver
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapestEver(
    std::string origin, std::string destinations, std::string departure_date_from,
    std::string departure_date_to, std::string departure_days_of_week, std::string return_date_from,
    std::string return_date_to, std::string return_days_of_week, uint16_t stay_from,
    uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from, uint32_t price_to,
    uint16_t limit, uint32_t max_lifetime_sec, ::utils::Threelean roundtrip_flights)

{
  DealsCheapestByPeriod query(*db_index);  // <- table processed by search class

  // short for of applying all filters
  query.apply_filters(origin, destinations, departure_date_from, departure_date_to,
                      departure_days_of_week, return_date_from, return_date_to, return_days_of_week,
                      stay_from, stay_to, direct_flights, price_from, price_to, limit,
                      max_lifetime_sec, roundtrip_flights);

  query.execute();

  // sort by price DESC if result greater than limit
  if (limit > 0 && limit < query.exec_result.size()) {
    std::sort(query.exec_result.begin(), query.exec_result.end(),
              [](const i::DealInfo &a, const i::DealInfo &b) { return a.price < b.price; });
    query.exec_result.resize(limit);
  }

  // load deals data from data pages (DealData shared memory pagers)
  std::vector<DealInfo> result = fill_deals_with_data(query.exec_result);

  return result;
}

//----------------------------------------------------------------
// DealsCheapestByPeriod PRESEARCH
//----------------------------------------------------------------
void DealsCheapestByPeriod::pre_search() {
  // free if was allocated before
  if (result_deals != nullptr) {
    delete result_deals;
  }
  // allocare space for result
  result_deals = new i::DealInfo[destination_values_size];
  // init first element
  max_price_deal = 0;
  deals_slots_used = 0;
  result_deals[0].price = 0;
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
bool DealsCheapestByPeriod::process_deal(const i::DealInfo &deal) {
  // ----------------------------------
  // try to find deal by destination in result array
  // ----------------------------------
  for (uint16_t fidx = 0; fidx < deals_slots_used; ++fidx) {
    i::DealInfo &result_deal = result_deals[fidx];

    if (deal.destination != result_deal.destination) {
      continue;
    }

    // we already have this destination, let's check for price
    if (deal.price < result_deal.price) {
      bool overriden = result_deal.flags.overriden;
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = overriden;
      // evaluate it here but not every compare itearation
      max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    }
    // if  not cheaper but same dates, replace with
    // newer results
    else if (deal.departure_date == result_deal.departure_date &&
             deal.return_date == result_deal.return_date) {
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = true;
      // evaluate it here but not every compare itearation
      max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    }

    // we was found destination, so goto the next deal element
    return true;
  }

  // ----------------------------------
  // no destinations are found in result
  // if there are unUsed slots -> fill them
  if (deals_slots_used < destination_values_size) {
    // deals::utils::print(deal);
    deals::utils::copy(result_deals[deals_slots_used], deal);
    deals_slots_used++;
    // evaluate it here but not every compare itearation
    max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    return true;
  }

  if (filter_destination) {
    // not possible situation
    std::cout << "filter destination exist but not found and all slots are used. but how?"
              << std::endl;
    throw "DealsCheapestByPeriod::process_deal.deals_slots_used.full";
  }

  // ----------------------------------
  // if all slots are used, but current deals
  // is cheaper than deals in result -> let replace most expensive with new
  // one (new destiantion is not in result_deals)
  if (deal.price < result_deals[max_price_deal].price) {
    deals::utils::copy(result_deals[max_price_deal], deal);
    // evaluate it here but not every compare itearation
    max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    return true;
  }

  // result_deals are full by "limit"
  // and current deal price is more than any price in result vector
  // (result_deals)
  // so just skip it
  return true;
}

//----------------------------------------------------------------
// DealsCheapestByPeriod POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestByPeriod::post_search() {
  // process results
  for (int i = 0; i < deals_slots_used; ++i) {
    exec_result.push_back(result_deals[i]);
  }
}

//----------------------------------------------------------------
// DealsCheapestByPeriod destructor
//----------------------------------------------------------------
DealsCheapestByPeriod::~DealsCheapestByPeriod() {
  if (result_deals != nullptr) {
    delete result_deals;
  }
}

//      ***************************************************
//                   CHEAPEST DAY BY DAY
//      ***************************************************

/*---------------------------------------------------------
* DealsDatabase  searchForCheapestDayByDay
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapestDayByDay(
    std::string origin, std::string destinations, std::string departure_date_from,
    std::string departure_date_to, std::string departure_days_of_week, std::string return_date_from,
    std::string return_date_to, std::string return_days_of_week, uint16_t stay_from,
    uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from, uint32_t price_to,
    uint16_t limit, uint32_t max_lifetime_sec, ::utils::Threelean roundtrip_flights) {
  //
  DealsCheapestDayByDay query(*db_index);

  query.apply_filters(origin, destinations, departure_date_from, departure_date_to,
                      departure_days_of_week, return_date_from, return_date_to, return_days_of_week,
                      stay_from, stay_to, direct_flights, price_from, price_to, limit,
                      max_lifetime_sec, roundtrip_flights);

  query.execute();

  std::vector<DealInfo> result = fill_deals_with_data(query.exec_result);

  std::sort(result.begin(), result.end(), [](const DealInfo &a, const DealInfo &b) {
    return a.departure_date < b.departure_date;
  });

  // for (auto& deal : result) {
  //   deals::utils::print(deal);
  // }

  return result;
}

//----------------------------------------------------------------
// DealsCheapestDayByDay PRESEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::pre_search() {
  // init values
  if (!filter_departure_date || !departure_date_values.duration) {
    std::cout << "ERROR no departure_date range" << std::endl;
    throw "zero interval. departure date interval must be specified";
  }

  deals_slots_available = destination_values_size * departure_date_values.duration;

  if (deals_slots_available > 1000) {
    std::cout << "ERROR deals_slots_available > 1000" << std::endl;
    throw "too much deals count requested, reduce";
  }
  // std::cout << "destination_values_size:" << destination_values_size << std::endl;
  // std::cout << "departure_date_values.duration:" << departure_date_values.duration << std::endl;
  // std::cout << "deals_slots_available:" << deals_slots_available << std::endl;

  if (result_deals != nullptr) {
    delete result_deals;
  }
  result_deals = new i::DealInfo[deals_slots_available];
  deals_slots_used = 0;
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
bool DealsCheapestDayByDay::process_deal(const i::DealInfo &deal) {
  // ----------------------------------
  // try to find deal by date and destination in result array
  // ----------------------------------
  uint16_t deals_with_current_date_count = 0;
  i::DealInfo *deals_with_current_date[destination_values_size];

  for (uint16_t fidx = 0; fidx < deals_slots_used; ++fidx) {
    i::DealInfo &result_deal = result_deals[fidx];

    if (deal.departure_date != result_deal.departure_date) {
      continue;
    }

    if (deals_with_current_date_count < destination_values_size) {
      // save pointer to deals with departure_date equal to processed one
      deals_with_current_date[deals_with_current_date_count++] = &result_deal;
    }

    if (deal.destination != result_deal.destination) {
      // below we check only date+destination already have been in top
      // so if it's new -> skip it
      continue;
    }

    // we already have this destination, let's check for price
    if (deal.price < result_deal.price) {
      bool overriden = result_deal.flags.overriden;
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = overriden;
    }
    // if price not cheaper but same dates, replace with
    // newer results
    else if (deal.departure_date == result_deal.departure_date &&
             deal.return_date == result_deal.return_date) {
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = true;
    }

    // we was found destination, so goto the next deal element
    return true;
  }

  if (deals_with_current_date_count >= destination_values_size) {
    // limit reached for this date. need to replace something
    uint16_t idx_max = deals::utils::get_max_price_in_pointers_array(deals_with_current_date,
                                                                     deals_with_current_date_count);
    if (deal.price < deals_with_current_date[idx_max]->price) {
      deals::utils::copy(*deals_with_current_date[idx_max], deal);
    }
    return true;
  }

  // ----------------------------------
  // no destinations are found in result
  // if there are unUsed slots -> fill them
  if (deals_slots_used < deals_slots_available) {
    // deals::utils::print(deal);
    deals::utils::copy(result_deals[deals_slots_used], deal);
    deals_slots_used++;
    return true;
  }

  std::cout << "\ndeals_slots_used:" << deals_slots_used
            << " deals_slots_available:" << deals_slots_available
            << " deals_with_current_date_count:" << deals_with_current_date_count
            << " destination_values_size:" << destination_values_size
            << " departure_date_values.duration:" << departure_date_values.duration
            << " ERROR very strange place. there will be exacly days*destinations deals,"
            << " no more. but it seems we found something extra?" << std::endl;

  return true;
}

//----------------------------------------------------------------
// DealsCheapestDayByDay POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::post_search() {
  // process results
  for (int i = 0; i < deals_slots_used; ++i) {
    if (result_deals[i].price > 0) {
      exec_result.push_back(result_deals[i]);
    }
  }
}

//----------------------------------------------------------------
// DealsCheapestDayByDay destructor
//----------------------------------------------------------------
DealsCheapestDayByDay::~DealsCheapestDayByDay() {
  if (result_deals != nullptr) {
    delete result_deals;
  }
}

//***********************************************************
//                   UTILS
//***********************************************************

void copy(i::DealInfo &dst, const i::DealInfo &src) {
  memcpy(&dst, &src, sizeof(i::DealInfo));
}

uint16_t get_max_price_in_array(i::DealInfo *&dst, uint16_t size) {
  // assert(size > 0);
  uint16_t max = 0;
  uint16_t pos = 0;

  for (uint16_t i = 0; i < size; ++i) {
    if (max < dst[i].price) {
      max = dst[i].price;
      pos = i;
    }
  }

  if (max == 0) {
    throw "get_max_price_in_array.max_not_found";
  }

  return pos;
}

uint16_t get_max_price_in_pointers_array(i::DealInfo *dst[], uint16_t size) {
  uint16_t max = 0;
  uint16_t pos = 0;

  for (uint16_t i = 0; i < size; ++i) {
    if (max < dst[i]->price) {
      max = dst[i]->price;
      pos = i;
    }
  }

  if (max == 0) {
    throw "get_max_price_in_pointers_array.max_not_found";
  }

  return pos;
}
