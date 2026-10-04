#include "config.hxx"
#include "csv.hxx"
#include "http.hxx"
#include "options.hxx"
#include "report.hxx"
#include "stats.hxx"
#include "tar.hxx"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace io = boost::iostreams;

void print_options(const Options& opt, const Config& cfg, const std::string& source) {
    std::cout << "year:      " << opt.year << "\n"
              << "station:   " << (opt.station.empty() ? "<all>" : opt.station) << "\n"
              << "config:    " << opt.config << "\n"
              << "top:       " << opt.top << "\n"
              << "unit:      " << (cfg.unit == Unit::Celsius ? "C" : "F") << "\n"
              << "base_url:  " << cfg.base_url << "\n"
              << "cache_dir: " << cfg.cache_dir.value_or("<none>") << "\n"
              << "source:    " << source << "\n";
}

// A local archive to read instead of downloading: --input, or <cache_dir>/<year>.tar.gz if it exists
auto find_local_archive(const Options& opt, const Config& cfg) -> std::optional<std::filesystem::path> {
    if (!opt.input.empty()) return opt.input;
    if (cfg.cache_dir) {
        auto cached = cache_file(*cfg.cache_dir, opt.year);
        if (std::filesystem::exists(cached)) return cached;
    }
    return std::nullopt;
}

auto describe(const io::gzip_error& e) -> std::string {
    switch (e.error()) {
        case io::gzip::bad_header: return "not a gzip file";
        case io::gzip::bad_method: return "unsupported gzip compression method";
        case io::gzip::bad_crc:    return "corrupt gzip data (checksum mismatch)";
        case io::gzip::bad_length: return "corrupt gzip data (length mismatch)";
        default:                   return "truncated or corrupt gzip data";
    }
}

// Unzip + untar + parse a .tar.gz from any Iostreams source (a file or an HTTPS
// download), and aggregate the stations that match the filter. The label is
// used in error messages.
template <typename Source>
auto collect_stations(Source source, const std::string& label, const std::string& filter, Unit unit) -> StationMap {
    auto in = io::filtering_istream();
    in.push(io::gzip_decompressor());
    in.push(std::move(source), 64 * 1024);
    in.exceptions(std::ios::badbit);    // otherwise errors from the decompressor are swallowed

    auto stations = StationMap();
    try {
        auto tar = TarReader(in);
        while (auto entry = tar.next()) {
            if (!entry->is_file()) continue;

            try {
                auto reader  = ObservationReader(tar.content());
                auto station = StationStats();
                while (auto obs = reader.next()) {
                    station.add(*obs, unit);
                }
                if (station_matches(filter, station.id(), station.name())) {
                    auto id = station.id();
                    stations.emplace(std::move(id), std::move(station));
                }
            } catch (const std::runtime_error& e) {
                throw std::runtime_error(entry->name + ": " + e.what());
            }
        }
    } catch (const io::gzip_error& e) {
        throw std::runtime_error(label + ": " + describe(e));
    }
    return stations;
}

// Up to this many stations get a monthly table, more than that gives the top lists
constexpr auto max_tables = std::size_t{5};

void print_elapsed(std::chrono::steady_clock::time_point start) {
    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start);
    std::cout << std::format("Elapsed time: {:.2f} s\n", elapsed.count());
}

// --download: fetch the archive of the year into the cache directory, and nothing more
void download_to_cache(const Options& opt, const Config& cfg) {
    if (!opt.input.empty()) throw std::runtime_error("--download cannot be combined with --input");
    if (!cfg.cache_dir) throw std::runtime_error("--download needs cache_dir in the config file " + opt.config);

    auto url    = archive_url(cfg.base_url, opt.year);
    auto target = cache_file(*cfg.cache_dir, opt.year);
    std::cout << "downloading " << url << " ..." << std::endl;

    auto bytes = download(url, target);
    std::cout << std::format("saved to {} ({:.1f} MB)\n", target.string(), static_cast<double>(bytes) / (1024 * 1024));
}

int main(int argc, char** argv) {
    try {
        auto start = std::chrono::steady_clock::now();
        auto opt   = Options();
        if (!parse_options(argc, argv, opt)) return 0;

        auto cfg = load_config(opt.config, opt.config_given);

        if (opt.download) {
            download_to_cache(opt, cfg);
            print_elapsed(start);
            return 0;
        }

        auto local = find_local_archive(opt, cfg);
        auto url   = archive_url(cfg.base_url, opt.year);
        auto source = !local            ? "downloading " + url
                    : opt.input.empty() ? "cached file " + local->string()
                    :                     "file " + local->string();
        print_options(opt, cfg, source);
        std::cout << "\n";

        auto stations = StationMap();
        if (local) {
            if (!std::filesystem::exists(*local)) throw std::runtime_error("cannot open " + local->string());
            stations = collect_stations(io::file_source(local->string(), std::ios::binary),
                                        local->string(), opt.station, cfg.unit);
        } else {
            stations = collect_stations(HttpSource(url), url, opt.station, cfg.unit);
        }
        if (stations.empty()) {
            throw std::runtime_error("no station matches '" + opt.station + "'");
        }

        if (stations.size() <= max_tables) {
            for (const auto& [id, station] : stations) {
                print_station(std::cout, station, cfg.unit, opt.year);
                std::cout << "\n";
            }
        } else {
            auto n = static_cast<std::size_t>(opt.top);
            print_ranking(std::cout, std::format("The {} hottest stations in {}", n, opt.year),
                          rank_stations(stations, n, Order::Hottest), cfg.unit);
            std::cout << "\n";
            print_ranking(std::cout, std::format("The {} coldest stations in {}", n, opt.year),
                          rank_stations(stations, n, Order::Coldest), cfg.unit);
            std::cout << "\n" << stations.size() << " stations, use --station to select up to "
                      << max_tables << " for a monthly table\n";
        }

        print_elapsed(start);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
