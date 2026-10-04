#include "stats.hxx"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <initializer_list>
#include <vector>

using Catch::Approx;
using namespace std::chrono;

namespace {
    auto summary_of(std::initializer_list<double> values) -> Summary {
        auto s = Summary();
        for (auto v : values) s.add(v);
        return s;
    }

    auto observation(year_month_day date, std::optional<double> temp, const std::string& name = "SOMEWHERE") -> Observation {
        auto obs    = Observation();
        obs.station = "123";
        obs.name    = name;
        obs.date    = date;
        obs.temp    = temp;
        return obs;
    }
}

TEST_CASE("units: Fahrenheit to Celsius", "[stats]") {
    CHECK(to_unit(32.0,  Unit::Celsius) == Approx(0.0).margin(1e-9));
    CHECK(to_unit(212.0, Unit::Celsius) == Approx(100.0));
    CHECK(to_unit(-40.0, Unit::Celsius) == Approx(-40.0));
    CHECK(to_unit(45.3,  Unit::Celsius) == Approx(7.4).margin(0.05));
}

TEST_CASE("units: Fahrenheit stays as it is", "[stats]") {
    CHECK(to_unit(45.3, Unit::Fahrenheit) == 45.3);
}

TEST_CASE("summary: nothing added", "[stats]") {
    auto s = Summary();
    CHECK(s.count() == 0);
    CHECK_FALSE(s.min().has_value());
    CHECK_FALSE(s.max().has_value());
    CHECK_FALSE(s.mean().has_value());
    CHECK_FALSE(s.stddev().has_value());
    CHECK_FALSE(s.median().has_value());
}

TEST_CASE("summary: one value", "[stats]") {
    auto s = summary_of({4.5});
    CHECK(s.count() == 1);
    CHECK(*s.min() == 4.5);
    CHECK(*s.max() == 4.5);
    CHECK(*s.mean() == Approx(4.5));
    CHECK(*s.stddev() == Approx(0.0).margin(1e-9));
    CHECK_FALSE(s.median().has_value());
}

TEST_CASE("summary: a few values", "[stats]") {
    auto s = summary_of({2, 4, 4, 4, 5, 5, 7, 9});          // the classic: mean 5, standard deviation 2
    CHECK(s.count() == 8);
    CHECK(*s.min() == 2);
    CHECK(*s.max() == 9);
    CHECK(*s.mean() == Approx(5.0));
    CHECK(*s.stddev() == Approx(2.0));
}

TEST_CASE("summary: negative values", "[stats]") {
    auto s = summary_of({-10, -20, -30});
    CHECK(*s.min() == -30);
    CHECK(*s.max() == -10);
    CHECK(*s.mean() == Approx(-20.0));
}

TEST_CASE("summary: the median needs five values", "[stats]") {
    auto s = Summary();
    for (auto v : {9.0, 1.0, 7.0, 3.0}) {
        s.add(v);
        CHECK_FALSE(s.median().has_value());
    }
    s.add(5.0);
    REQUIRE(s.median().has_value());
    CHECK(*s.median() == Approx(5.0));                      // exact for exactly five values
}

TEST_CASE("summary: median is estimated well for many values", "[stats]") {
    auto s = Summary();
    for (auto i = 0; i < 1001; ++i) {
        s.add(static_cast<double>((i * 7919) % 1001));      // 0..1000 in a scrambled order
    }
    REQUIRE(s.median().has_value());
    CHECK(*s.median() == Approx(500.0).margin(10.0));       // P² is an estimate, not exact
    CHECK(*s.mean() == Approx(500.0));
}

TEST_CASE("station: id and name come from the first observation", "[stats]") {
    auto st = StationStats();
    st.add(observation(2000y / March / 1, 50.0, "FIRST"), Unit::Fahrenheit);
    st.add(observation(2000y / March / 2, 51.0, "SECOND"), Unit::Fahrenheit);
    CHECK(st.id() == "123");
    CHECK(st.name() == "FIRST");
}

TEST_CASE("station: temperatures are collected per month and for the year", "[stats]") {
    auto st = StationStats();
    st.add(observation(2000y / January / 1,  32.0), Unit::Celsius);
    st.add(observation(2000y / January / 2,  50.0), Unit::Celsius);
    st.add(observation(2000y / July / 1,     86.0), Unit::Celsius);

    CHECK(st.month(1).count() == 2);
    CHECK(*st.month(1).min()  == Approx(0.0).margin(1e-9));
    CHECK(*st.month(1).max()  == Approx(10.0));
    CHECK(*st.month(1).mean() == Approx(5.0));
    CHECK(st.month(7).count() == 1);
    CHECK(*st.month(7).mean() == Approx(30.0));
    CHECK(st.month(2).count() == 0);
    CHECK(st.year().count() == 3);
    CHECK(*st.year().mean() == Approx(40.0 / 3.0));
}

TEST_CASE("station: Fahrenheit is kept when asked for", "[stats]") {
    auto st = StationStats();
    st.add(observation(2000y / May / 5, 77.0), Unit::Fahrenheit);
    CHECK(*st.year().mean() == Approx(77.0));
}

TEST_CASE("station: days without a temperature count as days only", "[stats]") {
    auto st = StationStats();
    st.add(observation(2000y / May / 5, 77.0), Unit::Fahrenheit);
    st.add(observation(2000y / May / 6, std::nullopt), Unit::Fahrenheit);
    CHECK(st.days() == 2);
    CHECK(st.month(5).count() == 1);
    CHECK(st.year().count() == 1);
}

TEST_CASE("station: month must be 1..12", "[stats]") {
    auto st = StationStats();
    CHECK_THROWS_AS(st.month(0), std::out_of_range);
    CHECK_THROWS_AS(st.month(13), std::out_of_range);
}

TEST_CASE("station filter", "[stats]") {
    auto id   = std::string("03005099999");
    auto name = std::string("LERWICK, UK");

    CHECK(station_matches("", id, name));                   // no filter, everything matches
    CHECK(station_matches("03005099999", id, name));
    CHECK(station_matches("030050", id, name));             // the start of the id
    CHECK(station_matches("lerwick", id, name));            // part of the name, any case
    CHECK(station_matches("Uk", id, name));
    CHECK_FALSE(station_matches("05099", id, name));        // the middle of the id is not enough
    CHECK(station_matches("wick", id, name));               // anywhere in the name
    CHECK_FALSE(station_matches("oslo", id, name));
    CHECK_FALSE(station_matches("lerwick", id, ""));        // station without a name
}

namespace {
    // A station with the given number of days in January, all with the same temperature in °C
    auto station(const std::string& id, std::size_t days, double celsius) -> StationStats {
        auto st = StationStats();
        for (auto d = 1u; d <= days; ++d) {
            auto obs    = Observation();
            obs.station = id;
            obs.name    = "NAME " + id;
            obs.date    = 2000y / January / std::chrono::day(d);
            obs.temp    = celsius * 9.0 / 5.0 + 32.0;
            st.add(obs, Unit::Celsius);
        }
        return st;
    }

    auto ids(const std::vector<const StationStats*>& v) -> std::vector<std::string> {
        auto result = std::vector<std::string>();
        for (const auto* s : v) result.push_back(s->id());
        return result;
    }

    auto map_of(std::initializer_list<StationStats> list) -> StationMap {
        auto map = StationMap();
        for (const auto& st : list) map.emplace(st.id(), st);
        return map;
    }
}

TEST_CASE("ranking: hottest and coldest", "[stats]") {
    auto stations = map_of({station("a", 20, 10.0), station("b", 20, 30.0), station("c", 20, -5.0), station("d", 20, 20.0)});

    CHECK(ids(rank_stations(stations, 2, Order::Hottest)) == std::vector<std::string>{"b", "d"});
    CHECK(ids(rank_stations(stations, 2, Order::Coldest)) == std::vector<std::string>{"c", "a"});
}

TEST_CASE("ranking: fewer stations than asked for", "[stats]") {
    auto stations = map_of({station("a", 20, 10.0), station("b", 20, 30.0)});
    CHECK(ids(rank_stations(stations, 5, Order::Hottest)) == std::vector<std::string>{"b", "a"});
    CHECK(rank_stations(stations, 0, Order::Hottest).empty());
}

TEST_CASE("ranking: nothing to rank", "[stats]") {
    CHECK(rank_stations(StationMap(), 5, Order::Hottest).empty());
}

TEST_CASE("ranking: stations with few readings are left out", "[stats]") {
    // The best covered station has 20 days, so at least 10 are needed
    auto stations = map_of({station("full", 20, 10.0), station("half", 10, 40.0), station("short", 9, -40.0)});
    CHECK(ids(rank_stations(stations, 3, Order::Hottest)) == std::vector<std::string>{"half", "full"});
    CHECK(ids(rank_stations(stations, 3, Order::Coldest)) == std::vector<std::string>{"full", "half"});
}

TEST_CASE("ranking: stations without any temperature are left out", "[stats]") {
    auto empty = StationStats();
    auto obs    = Observation();
    obs.station = "none";
    obs.date    = 2000y / January / 1;
    empty.add(obs, Unit::Celsius);

    auto stations = map_of({station("a", 5, 10.0), empty});
    CHECK(ids(rank_stations(stations, 5, Order::Hottest)) == std::vector<std::string>{"a"});
}

TEST_CASE("ranking: equal means are ordered by station id", "[stats]") {
    auto stations = map_of({station("c", 5, 10.0), station("a", 5, 10.0), station("b", 5, 10.0)});
    CHECK(ids(rank_stations(stations, 3, Order::Hottest)) == std::vector<std::string>{"a", "b", "c"});
    CHECK(ids(rank_stations(stations, 3, Order::Coldest)) == std::vector<std::string>{"a", "b", "c"});
}
