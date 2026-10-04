#pragma once
#include "config.hxx"
#include "stats.hxx"
#include <ostream>
#include <string>
#include <vector>

// A horizontal bar for a value, with a vertical line at zero. The scale runs from
// lo to hi (lo <= 0 <= hi) over width characters, negative values extend to the
// left of the zero line and positive ones to the right.
auto make_bar(double value, double lo, double hi, int width) -> std::string;

// A table with the temperatures of each month of the year, for one station,
// with a bar chart of the monthly means
void print_station(std::ostream& out, const StationStats& station, Unit unit, int year);

// A numbered list of stations with their yearly mean temperature
void print_ranking(std::ostream& out, const std::string& title, const std::vector<const StationStats*>& stations, Unit unit);
