#pragma once
#include <filesystem>
#include <optional>
#include <string>

enum class Unit { Celsius, Fahrenheit };

struct Config {
    Unit                       unit     = Unit::Celsius;
    std::string                base_url = "https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive";
    std::optional<std::string> cache_dir;
};

// Reads an INI file. A missing file is only an error if required is true,
// otherwise the defaults are used.
auto load_config(const std::string& path, bool required) -> Config;

// Where the archive of a year is kept in the cache directory: <cache_dir>/<year>.tar.gz
auto cache_file(const std::string& cache_dir, int year) -> std::filesystem::path;
