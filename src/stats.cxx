#include "stats.hxx"
#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <cmath>
#include <ranges>

namespace acc = boost::accumulators;
namespace ba  = boost::algorithm;

auto to_unit(double fahrenheit, Unit unit) -> double {
    return unit == Unit::Fahrenheit ? fahrenheit : (fahrenheit - 32.0) * 5.0 / 9.0;
}

void Summary::add(double value) {
    acc_(value);
}

auto Summary::count() const -> std::size_t {
    return acc::count(acc_);
}

auto Summary::min() const -> std::optional<double> {
    if (count() == 0) return std::nullopt;
    return acc::min(acc_);
}

auto Summary::max() const -> std::optional<double> {
    if (count() == 0) return std::nullopt;
    return acc::max(acc_);
}

auto Summary::mean() const -> std::optional<double> {
    if (count() == 0) return std::nullopt;
    return acc::mean(acc_);
}

auto Summary::stddev() const -> std::optional<double> {
    if (count() == 0) return std::nullopt;
    return std::sqrt(std::max(0.0, acc::variance(acc_)));       // rounding can give a tiny negative
}

auto Summary::median() const -> std::optional<double> {
    // The P² algorithm needs five values to get started
    if (count() < 5) return std::nullopt;
    return acc::median(acc_);
}

void StationStats::add(const Observation& obs, Unit unit) {
    if (id_.empty()) {
        id_   = obs.station;
        name_ = obs.name;
    }
    ++days_;
    if (!obs.temp) return;

    auto temp = to_unit(*obs.temp, unit);
    months_[static_cast<unsigned>(obs.date.month()) - 1].add(temp);
    year_.add(temp);
}

auto station_matches(const std::string& filter, const std::string& id, const std::string& name) -> bool {
    return filter.empty() || ba::istarts_with(id, filter) || ba::icontains(name, filter);
}

auto rank_stations(const StationMap& stations, std::size_t n, Order order) -> std::vector<const StationStats*> {
    auto best = std::size_t{0};
    for (const auto& [id, station] : stations) best = std::max(best, station.year().count());

    auto eligible = std::vector<const StationStats*>();
    for (const auto& [id, station] : stations) {
        if (station.year().count() > 0 && station.year().count() * 2 >= best) eligible.push_back(&station);
    }

    auto before = [order](const StationStats* a, const StationStats* b) {
        auto mean_a = *a->year().mean();
        auto mean_b = *b->year().mean();
        if (mean_a != mean_b) return order == Order::Hottest ? mean_a > mean_b : mean_a < mean_b;
        return a->id() < b->id();                           // the same mean, keep the order stable
    };
    auto count = std::min(n, eligible.size());
    std::ranges::partial_sort(eligible, eligible.begin() + static_cast<std::ptrdiff_t>(count), before);
    eligible.resize(count);
    return eligible;
}
