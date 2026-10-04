#pragma once
#include <string>

struct Options {
    int         year    = 1929;
    std::string station;
    std::string config  = "boost-demo.ini";
    int         top     = 5;
    std::string input;
    bool        download = false;       // only download the archive to cache_dir, then exit
    bool        config_given = false;   // true if --config was on the command line
};

// Parses the command line into opt. Returns false if the program should exit
// (help was requested and printed). Throws boost::program_options::error.
auto parse_options(int argc, char** argv, Options& opt) -> bool;
