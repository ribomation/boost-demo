#include "csv.hxx"
#include <boost/algorithm/string/trim.hpp>
#include <boost/tokenizer.hpp>
#include <charconv>
#include <format>
#include <stdexcept>
#include <unordered_map>

namespace ba = boost::algorithm;

namespace {
    // Marker values for "no measurement" in GSOD
    constexpr auto missing_temp = 9999.9;   // TEMP, MAX, MIN
    constexpr auto missing_prcp = 99.99;    // PRCP

    auto number(const std::string& text) -> double {
        auto value = 0.0;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || end != text.data() + text.size()) {
            throw std::runtime_error("invalid number '" + text + "'");
        }
        return value;
    }

    auto measurement(const std::string& text, double missing) -> std::optional<double> {
        auto value = number(text);
        if (value == missing) return std::nullopt;
        return value;
    }

    // YYYY-MM-DD
    auto date(const std::string& text) -> std::chrono::year_month_day {
        auto bad = [&] { return std::runtime_error("invalid date '" + text + "'"); };
        if (text.size() != 10 || text[4] != '-' || text[7] != '-') throw bad();

        auto part = [&](std::size_t pos, std::size_t len) {
            auto value = 0;
            auto [end, ec] = std::from_chars(text.data() + pos, text.data() + pos + len, value);
            if (ec != std::errc{} || end != text.data() + pos + len) throw bad();
            return value;
        };
        auto ymd = std::chrono::year_month_day(
            std::chrono::year(part(0, 4)),
            std::chrono::month(static_cast<unsigned>(part(5, 2))),
            std::chrono::day(static_cast<unsigned>(part(8, 2))));
        if (!ymd.ok()) throw bad();
        return ymd;
    }
}

auto split_csv(const std::string& line) -> std::vector<std::string> {
    using Separator = boost::escaped_list_separator<char>;
    auto tokens = boost::tokenizer<Separator>(line, Separator("", ",", "\""));  // "" = no escape char

    auto fields = std::vector<std::string>();
    for (const auto& token : tokens) {
        fields.push_back(ba::trim_copy(token));         // the numbers are padded with spaces
    }
    return fields;
}

ObservationReader::ObservationReader(std::istream& in) : in_(in) {
    auto line = std::string();
    if (!std::getline(in_, line)) throw std::runtime_error("missing header line");
    ba::trim(line);

    auto header  = split_csv(line);
    field_count_ = header.size();

    auto columns = std::unordered_map<std::string, std::size_t>();
    for (auto i = std::size_t{0}; i < header.size(); ++i) columns[header[i]] = i;

    auto column = [&](const std::string& name) {
        auto it = columns.find(name);
        if (it == columns.end()) throw std::runtime_error("missing column " + name);
        return it->second;
    };
    station_ = column("STATION");
    name_    = column("NAME");
    date_    = column("DATE");
    temp_    = column("TEMP");
    max_     = column("MAX");
    min_     = column("MIN");
    prcp_    = column("PRCP");
}

auto ObservationReader::parse(const std::vector<std::string>& fields) const -> Observation {
    if (fields.size() != field_count_) {
        throw std::runtime_error(std::format("expected {} fields, found {}", field_count_, fields.size()));
    }
    return Observation{
        .station = fields[station_],
        .name    = fields[name_],
        .date    = date(fields[date_]),
        .temp    = measurement(fields[temp_], missing_temp),
        .max     = measurement(fields[max_],  missing_temp),
        .min     = measurement(fields[min_],  missing_temp),
        .prcp    = measurement(fields[prcp_], missing_prcp),
    };
}

auto ObservationReader::next() -> std::optional<Observation> {
    auto line = std::string();
    while (std::getline(in_, line)) {
        ++line_no_;
        ba::trim(line);                                 // also removes a trailing CR
        if (line.empty()) continue;

        try {
            return parse(split_csv(line));
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(std::format("line {}: {}", line_no_, e.what()));
        }
    }
    return std::nullopt;
}
