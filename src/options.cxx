#include "options.hxx"
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

auto parse_options(int argc, char** argv, Options& opt) -> bool {
    auto desc = po::options_description("Usage: boost-demo [options]");
    desc.add_options()
        ("help,h", "show this help")
        ("year,y", po::value(&opt.year)->default_value(opt.year), "year to download")
        ("station,s", po::value(&opt.station), "station id or name pattern (default: all)")
        ("config,c", po::value(&opt.config)->default_value(opt.config), "configuration file")
        ("input,i", po::value(&opt.input), "local .tar.gz file instead of downloading")
        ("download,d", po::bool_switch(&opt.download), "only download the archive of the year to cache_dir")
        ("top,n", po::value(&opt.top)->default_value(opt.top), "number of stations in the hottest and coldest lists")
        ;

    auto vm = po::variables_map();
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.contains("help")) {
        std::cout << desc << "\n";
        return false;
    }
    po::notify(vm);
    opt.config_given = !vm["config"].defaulted();
    return true;
}
