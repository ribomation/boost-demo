#include "csv.hxx"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>
#include <sstream>
#include <string>
#include <vector>

using Fields = std::vector<std::string>;
using Catch::Approx;
using namespace std::chrono;

namespace {
    const auto header = std::string(
        R"("STATION","DATE","LATITUDE","LONGITUDE","ELEVATION","NAME","TEMP","TEMP_ATTRIBUTES","DEWP","DEWP_ATTRIBUTES",)"
        R"("SLP","SLP_ATTRIBUTES","STP","STP_ATTRIBUTES","VISIB","VISIB_ATTRIBUTES","WDSP","WDSP_ATTRIBUTES","MXSPD","GUST",)"
        R"("MAX","MAX_ATTRIBUTES","MIN","MIN_ATTRIBUTES","PRCP","PRCP_ATTRIBUTES","SNDP","FRSHTT")");

    // Real lines from the 1929 file of Lerwick, UK
    const auto lerwick_1 = std::string(
        R"("03005099999","1929-10-01","60.1333333","-1.1833333","84.0","LERWICK, UK","  45.3"," 4","  40.0"," 4",)"
        R"("1001.6"," 4","999.9"," 0"," 17.1"," 4","  4.5"," 4","  8.9","999.9","  51.1"," ","  44.1","*"," 0.00","I","999.9","000000")");
    const auto lerwick_2 = std::string(
        R"("03005099999","1929-10-02","60.1333333","-1.1833333","84.0","LERWICK, UK","  49.5"," 4","  45.2"," 4",)"
        R"(" 977.6"," 4","999.9"," 0","  9.3"," 4"," 17.5"," 4"," 29.9","999.9","  53.1","*","  44.1"," ","99.99"," ","999.9","010000")");

    // A line from 1960 where station position and name are missing (unquoted empty fields)
    const auto no_name = std::string(
        R"("03669099999","1960-01-01",,,,,"  49.1"," 8","  47.1"," 8","1000.5"," 8","999.9"," 0","  7.2"," 8")"
        R"(," 10.2"," 8"," 14.0","999.9","  51.1"," ","  46.0","*","99.99"," ","999.9","010000")");

    auto lines(const std::vector<std::string>& v) -> std::string {
        auto text = std::string();
        for (const auto& line : v) text += line + "\n";
        return text;
    }

    auto read_all(const std::string& text) -> std::vector<Observation> {
        auto in     = std::istringstream(text);
        auto reader = ObservationReader(in);
        auto all    = std::vector<Observation>();
        while (auto obs = reader.next()) all.push_back(*obs);
        return all;
    }
}

TEST_CASE("csv: plain fields", "[csv]") {
    CHECK(split_csv("a,b,c") == Fields{"a", "b", "c"});
    CHECK(split_csv("single") == Fields{"single"});
}

TEST_CASE("csv: quoted fields", "[csv]") {
    CHECK(split_csv(R"("a","b","c")") == Fields{"a", "b", "c"});
    CHECK(split_csv(R"("LERWICK, UK","x")") == Fields{"LERWICK, UK", "x"});
    CHECK(split_csv(R"(1,"a,b,c",2)") == Fields{"1", "a,b,c", "2"});
}

TEST_CASE("csv: fields are trimmed", "[csv]") {
    CHECK(split_csv(R"("  45.3"," 4","1001.6 ")") == Fields{"45.3", "4", "1001.6"});
    CHECK(split_csv("  a , b ") == Fields{"a", "b"});
}

TEST_CASE("csv: empty fields are kept", "[csv]") {
    CHECK(split_csv("a,,c") == Fields{"a", "", "c"});
    CHECK(split_csv(R"("a","","c")") == Fields{"a", "", "c"});
    CHECK(split_csv("a,b,") == Fields{"a", "b", ""});
    CHECK(split_csv(",a") == Fields{"", "a"});
}

TEST_CASE("csv: backslash is just a character", "[csv]") {
    CHECK(split_csv(R"("a\b",c)") == Fields{R"(a\b)", "c"});
}

TEST_CASE("csv: header and a real record", "[csv]") {
    auto all = read_all(lines({header, lerwick_1}));
    REQUIRE(all.size() == 1);
    CHECK(all[0].station == "03005099999");
    CHECK(all[0].name == "LERWICK, UK");
    CHECK(all[0].date == 1929y / October / 1);
    REQUIRE(all[0].temp.has_value());
    CHECK(*all[0].temp == Approx(45.3));
    CHECK(*all[0].max == Approx(51.1));
    CHECK(*all[0].min == Approx(44.1));
    CHECK(*all[0].prcp == Approx(0.0));
}

TEST_CASE("csv: several records, in order", "[csv]") {
    auto all = read_all(lines({header, lerwick_1, lerwick_2}));
    REQUIRE(all.size() == 2);
    CHECK(all[0].date == 1929y / October / 1);
    CHECK(all[1].date == 1929y / October / 2);
    CHECK(*all[1].temp == Approx(49.5));
}

TEST_CASE("csv: missing measurements are nullopt", "[csv]") {
    SECTION("precipitation 99.99") {
        auto all = read_all(lines({header, lerwick_2}));
        REQUIRE(all.size() == 1);
        CHECK_FALSE(all[0].prcp.has_value());
        CHECK(all[0].temp.has_value());
    }
    SECTION("temperatures 9999.9") {
        auto text = lines({header,
            R"("1","2000-01-01","0","0","0","X","9999.9"," 0","0"," 0","0"," 0","0"," 0","0"," 0","0"," 0","0","0","9999.9"," ","9999.9"," ","0.00","I","0","0")"});
        auto all = read_all(text);
        REQUIRE(all.size() == 1);
        CHECK_FALSE(all[0].temp.has_value());
        CHECK_FALSE(all[0].max.has_value());
        CHECK_FALSE(all[0].min.has_value());
        CHECK(all[0].prcp.has_value());
    }
    SECTION("999.9 is a valid temperature elsewhere, only 9999.9 is the marker") {
        auto text = lines({header,
            R"("1","2000-01-01","0","0","0","X","999.9"," 0","0"," 0","0"," 0","0"," 0","0"," 0","0"," 0","0","0","0.0"," ","0.0"," ","0.00","I","0","0")"});
        CHECK(*read_all(text)[0].temp == Approx(999.9));
    }
}

TEST_CASE("csv: unquoted empty fields give an empty name", "[csv]") {
    auto all = read_all(lines({header, no_name}));
    REQUIRE(all.size() == 1);
    CHECK(all[0].name.empty());
    CHECK(all[0].station == "03669099999");
    CHECK(*all[0].temp == Approx(49.1));
}

TEST_CASE("csv: column order does not matter", "[csv]") {
    auto text = lines({
        R"("PRCP","MIN","MAX","TEMP","NAME","DATE","STATION")",
        R"("1.50","30.0","50.0","40.0","SOMEWHERE","2001-02-03","123")"});
    auto all = read_all(text);
    REQUIRE(all.size() == 1);
    CHECK(all[0].station == "123");
    CHECK(all[0].name == "SOMEWHERE");
    CHECK(all[0].date == 2001y / February / 3);
    CHECK(*all[0].temp == Approx(40.0));
    CHECK(*all[0].max == Approx(50.0));
    CHECK(*all[0].min == Approx(30.0));
    CHECK(*all[0].prcp == Approx(1.5));
}

TEST_CASE("csv: Windows line endings and blank lines", "[csv]") {
    auto text = header + std::string("\r\n") + lerwick_1 + "\r\n\r\n" + lerwick_2 + "\r\n\n";
    CHECK(read_all(text).size() == 2);
}

TEST_CASE("csv: header only gives no observations", "[csv]") {
    CHECK(read_all(lines({header})).empty());
    CHECK(read_all(header).empty());        // not even a final newline
}

TEST_CASE("csv: malformed input", "[csv]") {
    using Catch::Matchers::ContainsSubstring;

    SECTION("empty input") {
        CHECK_THROWS_WITH(read_all(""), "missing header line");
    }
    SECTION("required column missing") {
        CHECK_THROWS_WITH(read_all(R"("STATION","DATE","NAME","TEMP","MAX","MIN")" "\n"), "missing column PRCP");
    }
    SECTION("wrong number of fields, with line number") {
        CHECK_THROWS_WITH(read_all(lines({header, lerwick_1, R"("03005099999","1929-10-02")"})),
                          "line 3: expected 28 fields, found 2");
    }
    SECTION("bad number") {
        auto bad = lerwick_1;
        bad.replace(bad.find("  45.3"), 6, "  4x.3");
        CHECK_THROWS_WITH(read_all(lines({header, bad})), ContainsSubstring("line 2: invalid number '4x.3'"));
    }
    SECTION("bad date") {
        for (auto date : {"1929-13-01", "1929-02-30", "29-10-01", "1929/10/01", "abcd-10-01"}) {
            CAPTURE(date);
            auto bad = lerwick_1;
            bad.replace(bad.find("1929-10-01"), 10, date);
            CHECK_THROWS_WITH(read_all(lines({header, bad})), ContainsSubstring("invalid date"));
        }
    }
}
