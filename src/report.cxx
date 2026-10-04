#include "report.hxx"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <string>

namespace {
    constexpr auto bar_width = 24;

    auto cell(const std::optional<double>& value) -> std::string {
        return value ? std::format("{:8.1f}", *value) : std::format("{:>8}", "-");
    }

    auto row(const std::string& label, const Summary& s) -> std::string {
        return std::format("{:<6}{:>5}{}{}{}{}{}", label, s.count(),
                           cell(s.min()), cell(s.mean()), cell(s.max()), cell(s.stddev()), cell(s.median()));
    }

    auto display_name(const StationStats& station) -> std::string {
        return station.name().empty() ? "<unnamed>" : station.name();
    }

    // Names are plain ASCII in the GSOD files, so cutting by bytes is fine
    auto shorten(const std::string& text, std::size_t width) -> std::string {
        return text.size() <= width ? text : text.substr(0, width - 1) + "~";
    }
}

auto make_bar(double value, double lo, double hi, int width) -> std::string {
    auto span = hi - lo;
    if (span <= 0.0) return "|";

    auto zero  = static_cast<std::size_t>(std::lround(-lo / span * width));
    auto count = static_cast<std::size_t>(std::lround(std::abs(value) / span * width));
    auto line  = std::string(static_cast<std::size_t>(width) + 1, ' ');
    line[zero] = '|';
    for (auto i = std::size_t{1}; i <= count; ++i) {
        if (value >= 0.0 && zero + i < line.size()) line[zero + i] = '#';
        if (value <  0.0 && i <= zero)              line[zero - i] = '#';
    }
    line.erase(line.find_last_not_of(' ') + 1);
    return line;
}

void print_station(std::ostream& out, const StationStats& station, Unit unit, int year) {
    out << std::format("{} ({}), {}\n", display_name(station), station.id(), year);
    if (station.year().count() == 0) {
        out << "no temperature readings\n";
        return;
    }

    // The scale of the bars covers the monthly means, and always includes zero
    auto lo = 0.0, hi = 0.0;
    for (auto m = 1u; m <= 12; ++m) {
        if (auto mean = station.month(m).mean()) {
            lo = std::min(lo, *mean);
            hi = std::max(hi, *mean);
        }
    }

    out << std::format("Daily mean temperature in °{}\n", unit == Unit::Celsius ? 'C' : 'F');
    out << std::format("{:<6}{:>5}{:>8}{:>8}{:>8}{:>8}{:>8}  {}\n",
                       "Month", "Days", "Min", "Mean", "Max", "StdDev", "Median", "Monthly mean");
    for (auto m = 1u; m <= 12; ++m) {
        const auto& month = station.month(m);
        if (month.count() == 0) continue;
        out << row(std::format("{:%b}", std::chrono::month(m)), month)
            << "  " << make_bar(*month.mean(), lo, hi, bar_width) << "\n";
    }
    out << row("Year", station.year()) << "\n";
}

void print_ranking(std::ostream& out, const std::string& title, const std::vector<const StationStats*>& stations, Unit unit) {
    out << title << "\n";
    out << std::format("{:>4}  {:<13}{:<30}{:>5}{:>9}\n", "", "Station", "Name", "Days", unit == Unit::Celsius ? "Mean °C" : "Mean °F");
    auto n = 0;
    for (const auto* station : stations) {
        out << std::format("{:3}]  {:<13}{:<30}{:>5}{}\n",
                           ++n, station->id(), shorten(display_name(*station), 29), station->year().count(),
                           cell(station->year().mean()));
    }
}
