#include "config.hxx"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <stdexcept>

namespace pt = boost::property_tree;

auto load_config(const std::string& path, bool required) -> Config {
    auto cfg = Config();
    if (!std::filesystem::exists(path)) {
        if (required) throw std::runtime_error("cannot find config file '" + path + "'");
        return cfg;
    }

    auto tree = pt::ptree();
    pt::read_ini(path, tree);

    auto unit = tree.get("output.unit", std::string("C"));
    if      (unit == "C") cfg.unit = Unit::Celsius;
    else if (unit == "F") cfg.unit = Unit::Fahrenheit;
    else throw std::runtime_error("output.unit must be C or F, not '" + unit + "'");

    cfg.base_url  = tree.get("source.base_url", cfg.base_url);
    if (auto dir = tree.get_optional<std::string>("source.cache_dir")) {
        cfg.cache_dir = *dir;
    }
    return cfg;
}

auto cache_file(const std::string& cache_dir, int year) -> std::filesystem::path {
    return std::filesystem::path(cache_dir) / (std::to_string(year) + ".tar.gz");
}
