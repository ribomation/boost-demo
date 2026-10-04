#include "options.hxx"
#include <boost/program_options.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace po = boost::program_options;

namespace {
    // Runs parse_options() as if "boost-demo <args...>" was typed on the command line
    auto parse(std::vector<std::string> args, Options& opt) -> bool {
        args.insert(args.begin(), "boost-demo");
        auto argv = std::vector<char*>();
        for (auto& a : args) argv.push_back(a.data());
        return parse_options(static_cast<int>(argv.size()), argv.data(), opt);
    }
}

TEST_CASE("options: defaults", "[options]") {
    auto opt = Options();
    REQUIRE(parse({}, opt));
    CHECK(opt.year == 1929);
    CHECK(opt.station.empty());
    CHECK(opt.config == "boost-demo.ini");
    CHECK(opt.top == 5);
    CHECK(opt.input.empty());
    CHECK_FALSE(opt.download);
    CHECK_FALSE(opt.config_given);
}

TEST_CASE("options: long names", "[options]") {
    auto opt = Options();
    REQUIRE(parse({"--year", "1950", "--station=lerwick", "--top", "10", "--input", "x.tar.gz"}, opt));
    CHECK(opt.year == 1950);
    CHECK(opt.station == "lerwick");
    CHECK(opt.top == 10);
    CHECK(opt.input == "x.tar.gz");
}

TEST_CASE("options: short names", "[options]") {
    auto opt = Options();
    REQUIRE(parse({"-y", "1960", "-s", "03005099999", "-n", "3", "-i", "y.tar.gz"}, opt));
    CHECK(opt.year == 1960);
    CHECK(opt.station == "03005099999");
    CHECK(opt.top == 3);
    CHECK(opt.input == "y.tar.gz");
}

TEST_CASE("options: download is a switch without a value", "[options]") {
    SECTION("long") {
        auto opt = Options();
        REQUIRE(parse({"--download", "--year", "1950"}, opt));
        CHECK(opt.download);
        CHECK(opt.year == 1950);
    }
    SECTION("short") {
        auto opt = Options();
        REQUIRE(parse({"-d"}, opt));
        CHECK(opt.download);
    }
}

TEST_CASE("options: config_given tells explicit from default", "[options]") {
    SECTION("not on the command line") {
        auto opt = Options();
        REQUIRE(parse({"-y", "1940"}, opt));
        CHECK_FALSE(opt.config_given);
    }
    SECTION("explicit, even if it names the default file") {
        auto opt = Options();
        REQUIRE(parse({"-c", "boost-demo.ini"}, opt));
        CHECK(opt.config_given);
        CHECK(opt.config == "boost-demo.ini");
    }
    SECTION("explicit, other file") {
        auto opt = Options();
        REQUIRE(parse({"--config", "other.ini"}, opt));
        CHECK(opt.config_given);
        CHECK(opt.config == "other.ini");
    }
}

TEST_CASE("options: help prints usage and asks the program to exit", "[options]") {
    auto out = std::ostringstream();
    auto* old = std::cout.rdbuf(out.rdbuf());
    auto opt = Options();
    auto proceed = parse({"--help"}, opt);
    std::cout.rdbuf(old);

    CHECK_FALSE(proceed);
    for (auto name : {"--year", "--station", "--config", "--input", "--download", "--top"}) {
        CHECK(out.str().find(name) != std::string::npos);
    }
}

TEST_CASE("options: errors", "[options]") {
    auto opt = Options();
    CHECK_THROWS_AS(parse({"--year", "abc"}, opt), po::invalid_option_value);
    CHECK_THROWS_AS(parse({"--top", "x"}, opt), po::invalid_option_value);
    CHECK_THROWS_AS(parse({"--bogus"}, opt), po::unknown_option);
    CHECK_THROWS_AS(parse({"--year"}, opt), po::invalid_command_line_syntax);
}
