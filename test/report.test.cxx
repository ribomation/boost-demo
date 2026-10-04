#include "report.hxx"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

using namespace std::chrono;

namespace {
    auto observation(year_month_day date, std::optional<double> temp,
                     const std::string& station = "123", const std::string& name = "TEST, XX") -> Observation {
        auto obs    = Observation();
        obs.station = station;
        obs.name    = name;
        obs.date    = date;
        obs.temp    = temp;
        return obs;
    }

    // Five October days at 32, 41, 50, 59 and 68 °F, which are exactly 0, 5, 10, 15 and 20 °C
    auto october() -> StationStats {
        auto st = StationStats();
        auto f  = 32.0;
        for (auto day = 1u; day <= 5; ++day, f += 9.0) {
            st.add(observation(2001y / October / day, f), Unit::Celsius);
        }
        return st;
    }

    auto table(const StationStats& st, Unit unit) -> std::string {
        auto out = std::ostringstream();
        print_station(out, st, unit, 2001);
        return out.str();
    }
}

TEST_CASE("report: station table", "[report]") {
    CHECK(table(october(), Unit::Celsius) ==
        "TEST, XX (123), 2001\n"
        "Daily mean temperature in °C\n"
        "Month  Days     Min    Mean     Max  StdDev  Median  Monthly mean\n"
        "Oct       5     0.0    10.0    20.0     7.1    10.0  |########################\n"
        "Year      5     0.0    10.0    20.0     7.1    10.0\n");
}

TEST_CASE("report: only months with readings are shown", "[report]") {
    auto st = october();
    st.add(observation(2001y / December / 24, 14.0), Unit::Celsius);

    auto text = table(st, Unit::Celsius);
    CHECK(text.find("Oct ") != std::string::npos);
    CHECK(text.find("Dec ") != std::string::npos);
    CHECK(text.find("Nov ") == std::string::npos);
    CHECK(text.find("Jan ") == std::string::npos);
}

TEST_CASE("report: numbers that need more data are shown as a dash", "[report]") {
    auto st = StationStats();
    st.add(observation(2001y / October / 1, 32.0), Unit::Celsius);      // one value: no median

    CHECK(table(st, Unit::Celsius) ==
        "TEST, XX (123), 2001\n"
        "Daily mean temperature in °C\n"
        "Month  Days     Min    Mean     Max  StdDev  Median  Monthly mean\n"
        "Oct       1     0.0     0.0     0.0     0.0       -  |\n"
        "Year      1     0.0     0.0     0.0     0.0       -\n");
}

TEST_CASE("report: Fahrenheit", "[report]") {
    auto st = StationStats();
    st.add(observation(2001y / October / 1, 32.0), Unit::Fahrenheit);

    auto text = table(st, Unit::Fahrenheit);
    CHECK(text.find("in °F") != std::string::npos);
    CHECK(text.find("32.0") != std::string::npos);
}

TEST_CASE("report: station without a name or readings", "[report]") {
    auto st = StationStats();
    st.add(observation(2001y / May / 1, std::nullopt, "999", ""), Unit::Celsius);

    CHECK(table(st, Unit::Celsius) ==
        "<unnamed> (999), 2001\n"
        "no temperature readings\n");
}

TEST_CASE("report: bars are scaled to the monthly means of the station", "[report]") {
    auto st = StationStats();
    st.add(observation(2001y / January / 1, 14.0), Unit::Celsius);     // -10 °C
    st.add(observation(2001y / July / 1, 68.0), Unit::Celsius);        //  20 °C

    // The scale is -10..20 over 24 characters: the zero line after 8, then 16 for July
    auto text = table(st, Unit::Celsius);
    CHECK(text.find("Jan       1   -10.0   -10.0   -10.0     0.0       -  ########|\n") != std::string::npos);
    CHECK(text.find("Jul       1    20.0    20.0    20.0     0.0       -          |################\n") != std::string::npos);
}

TEST_CASE("bar: positive and negative values around the zero line", "[report]") {
    CHECK(make_bar( 5, -10, 10, 20) == "          |#####");
    CHECK(make_bar(-5, -10, 10, 20) == "     #####|");
    CHECK(make_bar( 0, -10, 10, 20) == "          |");
}

TEST_CASE("bar: full scale", "[report]") {
    CHECK(make_bar( 10, -10, 10, 20) == "          |##########");
    CHECK(make_bar(-10, -10, 10, 20) == "##########|");
}

TEST_CASE("bar: only positive or only negative values", "[report]") {
    CHECK(make_bar(10, 0, 20, 20) == "|##########");
    CHECK(make_bar(20, 0, 20, 20) == "|####################");
    CHECK(make_bar(-10, -20, 0, 20) == "          ##########|");          // the zero line is at the right edge
}

TEST_CASE("bar: no scale at all", "[report]") {
    CHECK(make_bar(0, 0, 0, 20) == "|");
}

TEST_CASE("report: ranking has one numbered line per station", "[report]") {
    auto b = StationStats();
    b.add(observation(2001y / May / 1, 41.0, "222", "BETA"), Unit::Celsius);
    auto a = StationStats();
    a.add(observation(2001y / May / 1, 50.0, "111", "ALPHA"), Unit::Celsius);

    auto out = std::ostringstream();
    print_ranking(out, "Some title", {&a, &b}, Unit::Celsius);
    CHECK(out.str() ==
        "Some title\n"
        "      Station      Name                           Days  Mean °C\n"
        "  1]  111          ALPHA                             1    10.0\n"
        "  2]  222          BETA                              1     5.0\n");
}

TEST_CASE("report: ranking with no stations is only a heading", "[report]") {
    auto out = std::ostringstream();
    print_ranking(out, "Title", {}, Unit::Fahrenheit);
    CHECK(out.str() ==
        "Title\n"
        "      Station      Name                           Days  Mean °F\n");
}

TEST_CASE("report: long names are shortened in the ranking", "[report]") {
    auto st = StationStats();
    st.add(observation(2001y / May / 1, 50.0, "111", "DUBUQUE REGIONAL AIRPORT, IA US"), Unit::Celsius);

    auto out = std::ostringstream();
    print_ranking(out, "T", {&st}, Unit::Celsius);
    CHECK(out.str().find("DUBUQUE REGIONAL AIRPORT, IA~") != std::string::npos);
    CHECK(out.str().find("IA US") == std::string::npos);
}
