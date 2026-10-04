#include "config.hxx"
#include <boost/property_tree/ini_parser.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
    // An INI file that is removed again when the test is done
    class TempFile {
        std::filesystem::path path_;
    public:
        explicit TempFile(const std::string& name, const std::string& content = "")
            : path_(std::filesystem::temp_directory_path() / ("boost-demo-test-" + name)) {
            std::ofstream(path_) << content;
        }
        ~TempFile() { std::filesystem::remove(path_); }
        TempFile(const TempFile&) = delete;
        auto operator=(const TempFile&) -> TempFile& = delete;

        auto path() const -> std::string { return path_.string(); }
    };
}

TEST_CASE("config: all values from the file", "[config]") {
    auto file = TempFile("all.ini",
        "[output]\n"
        "unit = F\n"
        "[source]\n"
        "base_url = https://example.com/gsod\n"
        "cache_dir = /var/cache/gsod\n");

    auto cfg = load_config(file.path(), true);
    CHECK(cfg.unit == Unit::Fahrenheit);
    CHECK(cfg.base_url == "https://example.com/gsod");
    REQUIRE(cfg.cache_dir.has_value());
    CHECK(*cfg.cache_dir == "/var/cache/gsod");
}

TEST_CASE("config: missing keys keep their defaults", "[config]") {
    auto file = TempFile("partial.ini", "[output]\nunit = C\n");

    auto cfg = load_config(file.path(), true);
    CHECK(cfg.unit == Unit::Celsius);
    CHECK(cfg.base_url == Config().base_url);
    CHECK_FALSE(cfg.cache_dir.has_value());
}

TEST_CASE("config: empty file gives the defaults", "[config]") {
    auto file = TempFile("empty.ini");

    auto cfg = load_config(file.path(), true);
    CHECK(cfg.unit == Unit::Celsius);
    CHECK(cfg.base_url == Config().base_url);
    CHECK_FALSE(cfg.cache_dir.has_value());
}

TEST_CASE("config: missing file", "[config]") {
    auto missing = (std::filesystem::temp_directory_path() / "boost-demo-test-nonexistent.ini").string();

    SECTION("is fine when not required") {
        auto cfg = load_config(missing, false);
        CHECK(cfg.unit == Unit::Celsius);
        CHECK_FALSE(cfg.cache_dir.has_value());
    }
    SECTION("is an error when required") {
        CHECK_THROWS_AS(load_config(missing, true), std::runtime_error);
    }
}

TEST_CASE("config: invalid unit", "[config]") {
    auto file = TempFile("unit.ini", "[output]\nunit = K\n");
    CHECK_THROWS_WITH(load_config(file.path(), false), "output.unit must be C or F, not 'K'");
}

TEST_CASE("config: malformed INI", "[config]") {
    auto file = TempFile("broken.ini", "[output\nunit\n");
    CHECK_THROWS_AS(load_config(file.path(), false), boost::property_tree::ini_parser_error);
}

TEST_CASE("config: cache file", "[config]") {
    CHECK(cache_file("cache", 1950) == std::filesystem::path("cache/1950.tar.gz"));
    CHECK(cache_file("/var/cache/gsod/", 1929) == std::filesystem::path("/var/cache/gsod/1929.tar.gz"));
    CHECK(cache_file(".", 2000) == std::filesystem::path("./2000.tar.gz"));
}
