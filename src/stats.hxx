#pragma once
#include "config.hxx"
#include "csv.hxx"
#include <array>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/container/flat_map.hpp>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Degrees Fahrenheit, as in the GSOD files, to the unit used in the report
auto to_unit(double fahrenheit, Unit unit) -> double;

namespace stats_detail {
    namespace acc = boost::accumulators;

    using Features = acc::stats<
        acc::tag::count,
        acc::tag::min,
        acc::tag::max,
        acc::tag::mean,
        acc::tag::variance,
        acc::tag::median(acc::with_p_square_quantile)>;

    using Accumulator = acc::accumulator_set<double, Features>;
}

// Descriptive statistics of a series of numbers, based on Boost.Accumulators.
// The accessors return nullopt when there is not enough data for the number.
class Summary {
    stats_detail::Accumulator acc_;

public:
    void add(double value);

    auto count()  const -> std::size_t;
    auto min()    const -> std::optional<double>;
    auto max()    const -> std::optional<double>;
    auto mean()   const -> std::optional<double>;
    auto stddev() const -> std::optional<double>;   // population standard deviation
    auto median() const -> std::optional<double>;   // an estimate, needs at least 5 values
};

// The temperature statistics of one station and year, per month and for the whole year.
// Temperatures are the daily means, converted to the given unit when added.
class StationStats {
    std::string             id_;
    std::string             name_;
    std::size_t             days_ = 0;
    std::array<Summary, 12> months_;
    Summary                 year_;

public:
    // The first observation decides the id and the name of the station
    void add(const Observation& obs, Unit unit);

    auto id()   const -> const std::string& { return id_; }
    auto name() const -> const std::string& { return name_; }
    auto days() const -> std::size_t { return days_; }          // observations, with or without a temperature
    auto month(unsigned m) const -> const Summary& { return months_.at(m - 1); }     // 1..12
    auto year() const -> const Summary& { return year_; }
};

using StationMap = boost::container::flat_map<std::string, StationStats>;      // by station id

// Does the station match what the user asked for with --station? An empty
// filter matches everything, otherwise the filter must be the start of the
// id or a part of the name (ignoring case).
auto station_matches(const std::string& filter, const std::string& id, const std::string& name) -> bool;

enum class Order { Hottest, Coldest };

// The n stations with the highest or lowest yearly mean temperature. Stations
// with few readings would distort the ranking, so a station needs at least half
// as many readings as the best covered station to take part.
auto rank_stations(const StationMap& stations, std::size_t n, Order order) -> std::vector<const StationStats*>;
