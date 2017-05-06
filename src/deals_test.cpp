#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <iostream>

#include "deals_database.hpp"
#include "timing.hpp"
#define TEST_ELEMENTS_COUNT 50000

//------------------------------------------------------------------------
// TESTING HELL
//------------------------------------------------------------------------
namespace deals {
static types::ObjectMap params;
static const std::string origins[] = {"MOW", "MAD", "BER", "LON", "PAR",
                                      "LAX", "LED", "FRA", "BAR", "JFK"};
static const std::string countries[] = {"AD", "AE", "AF", "AG", "AI", "RU", "AL",
                                        "AM", "AN", "AO", "IT", "GE", "FR"};

//------------------------------------------------------------------------
types::Required<types::IATACode> getRandomOrigin() {
  uint16_t place = rand() % (sizeof(origins) / sizeof(origins[0]));
  return {params, origins[place]};
}

//------------------------------------------------------------------------
types::Required<types::CountryCode> getRandomCountry() {
  uint16_t place = rand() % (sizeof(countries) / sizeof(countries[0]));
  return {params, countries[place]};
}

//------------------------------------------------------------------------
types::Required<types::Number> getRandomPrice(uint32_t minPrice) {
  uint32_t price = rand() & 0x0000FFFF;
  price += minPrice;

  if (price < minPrice) {
    std::cout << "ALARM!! " << minPrice << " " << price << std::endl;
  }

  types::ObjectMap priceParam;
  priceParam.add_object({"test", std::to_string(price)});
  return {priceParam, "test"};
}

//------------------------------------------------------------------------
types::Required<types::Date> getRandomDate(uint32_t year = 2016) {
  uint32_t month = (rand() & 0x00000003) + (rand() & 0x00000003) + (rand() & 0x00000003) + 1;
  uint32_t day = (rand() & 0x00000007) + (rand() & 0x00000007) + (rand() & 0x00000007) + 1;

  types::ObjectMap param;
  param.add_object({"test", types::int_to_date(year * 10000 + month * 100 + day)});
  return {param, "test"};
}

//------------------------------------------------------------------------
types::Optional<types::Date> getRandomDateOpt(uint32_t year = 2016) {
  uint32_t month = (rand() & 0x00000003) + (rand() & 0x00000003) + (rand() & 0x00000003) + 1;
  uint32_t day = (rand() & 0x00000007) + (rand() & 0x00000007) + (rand() & 0x00000007) + 1;

  types::ObjectMap param;
  param.add_object({"test", types::int_to_date(year * 10000 + month * 100 + day)});
  return {param, "test"};
}

//------------------------------------------------------------------------
types::Optional<types::Boolean> getRandomBool() {
  uint32_t value = rand() & 0x0000FFFF;

  types::ObjectMap param;
  param.add_object({"test", value > 0x00008000 ? "true" : "false"});
  return {param, "test"};
}

//------------------------------------------------------------------------
void convertertionsTest() {
  std::cout << "Origin encoder/decoder" << std::endl;
  for (int i = 0; i < 10; ++i) {
    uint32_t code = types::origin_to_code(origins[i]);
    std::string decode = types::code_to_origin(code);
    assert(origins[i] == decode);
  }

  std::cout << "Locale encoder/decoder" << std::endl;
  for (int i = 0; i < sizeof(countries) / sizeof(countries[0]); ++i) {
    auto code = types::country_to_code(countries[i]);
    std::string decode = types::code_to_country(code);
    assert(countries[i] == decode);
  }

  std::cout << "Date encoder/decoder\n";
  uint32_t code = types::date_to_int("2017-01-01");
  std::string date = types::int_to_date(code);
  assert(code == 20170101);
  assert(date == "2017-01-01");

  std::cout << "Country encoder/decoder\n";
  auto ru = types::country_to_code("RU");
  assert(ru == 184);
  assert(types::code_to_country(ru) == "RU");
  assert(types::country_to_code("RU") != types::country_to_code("US"));

  std::cout << "Date functions\n";
  assert(::utils::days_between_dates("2015-01-01", "2015-01-01") == 0);
  assert(::utils::days_between_dates("2015-01-01", "2016-01-01") == 365);
  assert(::utils::days_between_dates("2015-02-28", "2015-03-01") == 1);

  assert(types::Weekdays("mon,tue,sat").get_bitmask() == 0b00100011);
  assert(types::Weekdays("").isUndefined() == true);
  assert(types::Weekdays("sun,mon,tue,wed,thu,fri,sat").get_bitmask() == 0b01111111);
  assert(types::Weekdays("sun,mon,thu,fri,sat").get_bitmask() == 0b01111001);

  types::Date date1("2015-01-01");
  types::Date date2("2015-02-28");
  types::Date date3("2015-03-01");
  types::Date date4("2016-01-01");

  assert(date1.get_code() == 20150101);
  assert(date2.get_code() == 20150228);
  assert(date3.get_code() == 20150301);
  assert(date4.get_code() == 20160101);

  assert(date1.get_year() == 2015);
  assert(date1.get_month() == 01);
  assert(date1.get_day() == 01);

  assert(date2.get_year() == 2015);
  assert(date2.get_month() == 02);
  assert(date2.get_day() == 28);

  assert(date3.get_year() == 2015);
  assert(date3.get_month() == 03);
  assert(date3.get_day() == 01);

  assert(date4.get_year() == 2016);
  assert(date4.get_month() == 01);
  assert(date4.get_day() == 01);

  assert(types::Weekdays(date1).get_bitmask() == 0b00001000);
  assert(types::Weekdays(date2).get_bitmask() == 0b00100000);
  assert(types::Weekdays(date3).get_bitmask() == 0b01000000);

  assert(date1.days_after(date1) == 0);
  assert(date4.days_after(date1) == 365);
  assert(date2.days_after(date1) == 58);
  assert(date3.days_after(date2) == 1);

  assert(::utils::day_of_week_str_from_date("2016-06-25") == "sat");
  assert(::utils::day_of_week_str_from_date("2016-04-13") == "wed");
  assert(::utils::day_of_week_from_str("sat") == 5);
  assert(::utils::day_of_week_from_str("mon") == 0);
  assert(::utils::day_of_week_from_str("sun") == 6);
  assert(::utils::day_of_week_from_str("eff") == 7);
}

//------------------------------------------------------------------------
void unit_test() {
  convertertionsTest();

  DealsDatabase db;
  db.truncate();

  std::string dumb = "1, 2, 3, 4, 5, 6, 7, 8";
  std::string check = "7, 7, 7";

  timing::Timer timer("Adding test values");
  srand(timing::getTimestampSec());

  for (auto &origin : origins) {
    params.add_object({origin, origin});
  }
  for (auto &country : countries) {
    params.add_object({country, country});
  }
  params.add_object({"true", "true"});
  params.add_object({"false", "false"});
  params.add_object({"2016-05-01", "2016-05-01"});
  params.add_object({"2016-05-21", "2016-05-21"});
  params.add_object({"2016-06-01", "2016-06-01"});
  params.add_object({"2016-06-10", "2016-06-10"});
  params.add_object({"2016-06-11", "2016-06-11"});
  params.add_object({"2016-06-22", "2016-06-22"});
  params.add_object({"2016-06-23", "2016-06-23"});
  params.add_object({"2016-07-01", "2016-07-01"});
  params.add_object({"2016-07-15", "2016-07-15"});
  params.add_object({"5000", "5000"});
  params.add_object({"6000", "6000"});
  params.add_object({"7000", "7000"});

  using ri = types::Required<types::IATACode>;
  using ois = types::Optional<types::IATACodes>;
  using rc = types::Required<types::CountryCode>;
  using od = types::Optional<types::Date>;
  using rd = types::Required<types::Date>;
  using ob = types::Optional<types::Boolean>;
  using rn = types::Required<types::Number>;
  using on = types::Optional<types::Number>;
  using ow = types::Optional<types::Weekdays>;
  using oc = types::Optional<types::CountryCodes>;

  // add some data, that will be outdated
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomCountry(), getRandomDate(),
               getRandomDateOpt(), getRandomBool(), getRandomPrice(1000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomCountry(), getRandomDate(),
               getRandomDateOpt(), getRandomBool(), getRandomPrice(2000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomCountry(), getRandomDate(),
               getRandomDateOpt(), getRandomBool(), getRandomPrice(3000), dumb);
  }

  // go to the feature (+1000 seconds)
  timing::TimeLord time;
  time += 1000;

  // add data we will expect
  db.addDeal(ri(params, "MOW"), ri(params, "MAD"), rc(params, "IT"), rd(params, "2016-05-01"),
             od(params, "2016-05-21"), getRandomBool(), rn(params, "5000"), check);
  db.addDeal(ri(params, "MOW"), ri(params, "BER"), rc(params, "GE"), rd(params, "2016-06-01"),
             od(params, "2016-06-11"), getRandomBool(), rn(params, "6000"), check);
  db.addDeal(ri(params, "MOW"), ri(params, "PAR"), rc(params, "FR"), rd(params, "2016-07-01"),
             od(params, "2016-07-15"), getRandomBool(), rn(params, "7000"), check);

  time += 5;

  // add some good
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), ri(params, "MAD"), rc(params, "IT"), getRandomDate(2015),
               getRandomDateOpt(2015), getRandomBool(), getRandomPrice(5100), dumb);
    db.addDeal(getRandomOrigin(), ri(params, "BER"), rc(params, "GE"), getRandomDate(),
               getRandomDateOpt(), getRandomBool(), getRandomPrice(6200), dumb);
    db.addDeal(getRandomOrigin(), ri(params, "PAR"), rc(params, "FR"), getRandomDate(),
               getRandomDateOpt(), getRandomBool(), getRandomPrice(7200), dumb);

    // MAD will be 2016 here: and > 8000 price
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomCountry(), getRandomDate(),
               getRandomDateOpt(), getRandomBool(), getRandomPrice(8000), dumb);
  }

  timer.tick("test1 INIT");
  params.add_object({"AAA,PAR,BER,MAD", "AAA,PAR,BER,MAD"});
  params.add_object({"0", "0"});
  params.add_object({"4", "4"});
  params.add_object({"18", "18"});
  params.add_object({"10", "10"});
  params.add_object({"9100", "9100"});
  params.add_object({"19200", "19200"});
  params.add_object({"2000", "2000"});

  // 1st test ----------------------------
  // *********************************************************
  std::vector<DealInfo> result = db.searchFor<deals::SimplyCheapest>(
      ri(params, "MOW"), ois(params, "AAA,PAR,BER,MAD"), oc(params, "z"), od(params, "z"),
      od(params, "z"), ow(params, "z"), od(params, "z"), od(params, "z"), ow(params, "z"),
      on(params, "z"), on(params, "z"), ob(params, "z"), on(params, "z"), on(params, "10"),
      ob(params, "z"), od(params, "z"));

  timer.tick("test1 START");
  for (auto &deal : result) {
    deals::utils::print(deal);
  }
  assert(result.size() == 3);

  int city_count[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); ++i) {
    deals::utils::print(result[i]);
    if (result[i].test->destination == "MAD") {
      city_count[0]++;
      if (result[i].test->overriden) {
        assert(result[i].test->price > 5000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].test->price == 5000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].test->departure_date == "2016-05-01");
      assert(result[i].test->return_date == "2016-05-21");
    } else if (result[i].test->destination == "BER") {
      city_count[1]++;
      if (result[i].test->overriden) {
        assert(result[i].test->price > 6000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].test->price == 6000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].test->departure_date == "2016-06-01");
      assert(result[i].test->return_date == "2016-06-11");
    } else if (result[i].test->destination == "PAR") {
      city_count[2]++;
      if (result[i].test->overriden) {
        assert(result[i].test->price > 7000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].test->price == 7000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].test->departure_date == "2016-07-01");
      assert(result[i].test->return_date == "2016-07-15");
    }
  }

  timer.tick("test 1 FINISH");
  timer.tick("test 2 INIT");
  // 2nd test -------------------------------
  // *********************************************************
  result = db.searchFor<deals::SimplyCheapest>(
      ri(params, "MOW"), ois(params, "AAA,PAR,BER,MAD"), oc(params, "z"), od(params, "2016-06-01"),
      od(params, "2016-06-23"), ow(params, "z"), od(params, "2016-06-10"), od(params, "2016-06-22"),
      ow(params, "z"), on(params, "z"), on(params, "z"), ob(params, "z"), on(params, "z"),
      on(params, "10"), ob(params, "z"), od(params, "z"));

  timer.tick("test 2 START");
  for (auto &deal : result) {
    deals::utils::print(deal);
  }

  assert(result.size() <= 3);
  int city_count2[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); ++i) {
    deals::utils::print(result[i]);
    assert(result[i].test->departure_date >= "2016-06-01");
    assert(result[i].test->departure_date <= "2016-06-23");
    assert(result[i].test->return_date >= "2016-06-10");
    assert(result[i].test->return_date <= "2016-06-22");

    if (result[i].test->destination == "MAD") {
      city_count2[0]++;
      // madrid in this dates only over 8000
      assert(result[i].test->price >= 8000);
      assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");

    } else if (result[i].test->destination == "BER") {
      city_count2[1]++;
      if (result[i].test->overriden) {
        assert(result[i].test->price > 6000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].test->price == 6000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].test->departure_date == "2016-06-01");
      assert(result[i].test->return_date == "2016-06-11");

    } else if (result[i].test->destination == "PAR") {
      city_count2[2]++;
      // Paris in this dates only over 7200
      assert(result[i].test->price >= 7200);
      assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
    }
  }

  assert(city_count2[0] <= 1);
  assert(city_count2[1] == 1);
  assert(city_count2[2] <= 1);
  timer.tick("test 2 FINISH");
  //--------------
  // 3rd test -------------------------------
  // *********************************************************
  timer.tick("test 3 INIT");
  params.add_object({"thu,sat,sun", "thu,sat,sun"});
  params.add_object({"wed,sun,mon", "wed,sun,mon"});
  params.add_object({"ZW,RU,IT", "ZW,RU,IT"});

  result = db.searchFor<deals::SimplyCheapest>(
      ri(params, "MOW"), ois(params, "z"), oc(params, "ZW,RU,IT"), od(params, "z"), od(params, "z"),
      ow(params, "thu,sat,sun"), od(params, "z"), od(params, "z"), ow(params, "wed,sun,mon"),
      on(params, "4"), on(params, "18"), ob(params, "false"), on(params, "z"), on(params, "2000"),
      ob(params, "z"), od(params, "z"));
  timer.tick("test 3 START");
  std::cout << "search 3 result size:" << result.size() << std::endl;
  assert(result.size() > 0);

  auto countryRU = types::CountryCode("RU").get_code();
  auto countryIT = types::CountryCode("IT").get_code();

  for (int i = 0; i < result.size(); ++i) {
    deals::utils::print(result[i]);
    assert(result[i].test->destination_country == countryRU ||
           result[i].test->destination_country == countryIT);
    assert(result[i].test->stay_days >= 4 && result[i].test->stay_days <= 18);
    assert(result[i].test->direct == false);
    std::string dw = ::utils::day_of_week_str_from_bitmask(result[i].test->departure_day_of_week);
    std::string rw = ::utils::day_of_week_str_from_bitmask(result[i].test->return_day_of_week);
    assert(dw == "thu" || dw == "sat" || dw == "sun");
    assert(rw == "wed" || rw == "sun" || rw == "mon");
  }

  timer.tick("test 3 FINISH");
  std::cout << "DEALS OK" << std::endl;
}
}  // namespace deals_test
