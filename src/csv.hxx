#pragma once
#include <chrono>
#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <vector>

// Splits one line of comma separated values into trimmed fields. Fields may
// be quoted, in which case they can contain commas. There is no escape
// character, and a quote inside a quoted field is not supported.
auto split_csv(const std::string& line) -> std::vector<std::string>;

// One day at one weather station, from a GSOD file. Units are those of the
// file: degrees Fahrenheit and inches. Missing measurements are nullopt.
struct Observation {
    std::string                station;
    std::string                name;        // can be empty
    std::chrono::year_month_day date;
    std::optional<double>      temp;        // daily mean
    std::optional<double>      max;
    std::optional<double>      min;
    std::optional<double>      prcp;        // precipitation
};

// Reads the observations of one station file, from the header line to the
// end of the stream. The columns are found by name via the header line, so
// their order does not matter. Throws std::runtime_error on malformed input.
class ObservationReader {
    std::istream& in_;
    std::size_t   line_no_ = 1;             // the header is line 1
    std::size_t   field_count_ = 0;
    std::size_t   station_ = 0, name_ = 0, date_ = 0, temp_ = 0, max_ = 0, min_ = 0, prcp_ = 0;

    auto parse(const std::vector<std::string>& fields) const -> Observation;

public:
    explicit ObservationReader(std::istream& in);

    // The next observation, or nullopt at the end of the stream
    auto next() -> std::optional<Observation>;
};
