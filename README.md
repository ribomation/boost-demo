# boost-demo

[![Build and test](https://github.com/ribomation/boost-demo/actions/workflows/build.yml/badge.svg)](https://github.com/ribomation/boost-demo/actions/workflows/build.yml)

A command-line tool that downloads one year of NOAA weather data, decompresses
and unpacks it as a stream, extracts the temperature measurements, aggregates
them and prints a readable report. Nothing is written to disk, unless you ask
for the archive to be cached.

The program is a demo of several [Boost](https://www.boost.org/) libraries
working together in one pipeline.

## The pipeline

```
Beast (HTTPS GET, body read chunk by chunk)
  → custom Iostreams Source
  → gzip_decompressor            (filtering_istream)
  → tar reader                   (one stream per station file)
  → CSV parsing                  (String_algo / Tokenizer)
  → statistics per station/month (Accumulators)
  → report
```

| Step                          | Boost library                                   | Source                       |
|-------------------------------|-------------------------------------------------|------------------------------|
| Command-line options          | Program_options                                 | `src/options.cxx`            |
| Configuration (INI file)      | Property_tree                                   | `src/config.cxx`             |
| HTTPS download, streamed      | Beast (Asio + OpenSSL)                          | `src/http.cxx`               |
| Beast → decompression         | Iostreams, a custom `Source` device             | `src/http.hxx`               |
| gunzip                        | Iostreams `gzip_decompressor`                   | `src/app.cxx`                |
| Untar                         | none — a small hand-written reader  | `src/tar.hxx`                |
| Parse CSV lines               | String_algo, Tokenizer                          | `src/csv.cxx`                |
| Aggregate                     | Accumulators, Container (`flat_map`)            | `src/stats.cxx`              |
| Report                        | `std::format`                                   | `src/report.cxx`             |

## The data: NOAA GSOD

[Global Summary of the Day](https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive/)
from NCEI (formerly NCDC) has one `.tar.gz` per year, 1929–2025, with one CSV
file per weather station. Temperatures are in °F, and missing values are coded
as `999.9`. The archives range from 48 KB (1929, 21 stations) to over 100 MB
(2024), so 1929 is a good year for a quick test.

## Building

Requirements:

- A C++26 compiler (developed with GCC 16.1)
- CMake 3.28 or later, and preferably Ninja
- Boost 1.92, with the compiled libraries `program_options` and `iostreams`
  (the latter built with zlib)
- OpenSSL
- Catch2, for the unit tests

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBoost_ROOT=$HOME/Libs/Boost/latest
cmake --build build
ctest --test-dir build
```

The `CMakeLists.txt` finds Catch2 via a `Find` module in `~/Libs/CMake-modules`;
adjust `CMAKE_MODULE_PATH` if your Catch2 is installed elsewhere.

## Usage

```
Usage: boost-demo [options]:
  -h [ --help ]                         show this help
  -y [ --year ] arg (=1929)             year to download
  -s [ --station ] arg                  station id or name pattern (default:
                                        all)
  -c [ --config ] arg (=boost-demo.ini) configuration file
  -i [ --input ] arg                    local .tar.gz file instead of
                                        downloading
  -d [ --download ]                     only download the archive of the year
                                        to cache_dir
  -n [ --top ] arg (=5)                 number of stations in the hottest and
                                        coldest lists
```

The archive is read from, in order of preference: the file given with
`--input`, `<cache_dir>/<year>.tar.gz` if it exists, or else it is downloaded
over HTTPS and processed as it arrives. Use `--download` to fill the cache.

With up to five matching stations, the report shows a monthly table per station:

```
$ ./build/boost-demo -y 1929 -s LERWICK
...
LERWICK, UK (03005099999), 1929
Daily mean temperature in °C
Month  Days     Min    Mean     Max  StdDev  Median  Monthly mean
Oct      30     4.2     7.9     9.8     1.6     8.2  |########################
Nov      30     1.9     7.1    11.3     2.3     7.7  |######################
Dec      31     2.2     6.1     9.0     1.8     5.6  |##################
Year     91     1.9     7.0    11.3     2.1     7.2
```

With more stations, it shows the hottest and coldest ones instead:

```
$ ./build/boost-demo -y 1929
...
The 5 hottest stations in 1929
      Station      Name                           Days  Mean °C
  1]  03777099999  PURLEY OAKS, UK                 150    11.7
  2]  03795099999  LYMPNE, UK                      149    11.2
  3]  03497099999  GORLESTON, UK                   153    11.2
  4]  03856099999  PORTLAND BILL LH, UK             92    10.8
  5]  03894099999  GUERNSEY, GK                     89    10.6

The 5 coldest stations in 1929
      Station      Name                           Days  Mean °C
  1]  03075099999  WICK, UK                         92     6.5
  2]  03005099999  LERWICK, UK                      91     7.0
  3]  03159099999  INCHKEITH, UK                    91     7.1
  4]  03262099999  TYNEMOUTH, UK                    92     7.2
  5]  03396099999  SPURN HEAD POINT, UK             92     7.8

21 stations, use --station to select up to 5 for a monthly table
```

## Configuration

`boost-demo.ini` (or the file given with `--config`):

```ini
[output]
; Temperature unit in the report: C or F
unit = C

[source]
; The archive for a year is <base_url>/<year>.tar.gz
base_url = https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive
; Optional directory for locally cached archives
cache_dir = cache
```

If the default file is missing, built-in defaults are used.

## License

MIT, see [LICENSE](LICENSE).

## Authors

Jens Riboe, [Ribomation](https://www.ribomation.se/)

Co-author: Claude Opus 5.5, [Anthropic](https://www.anthropic.com/claude) (via [Claude Code](https://claude.com/claude-code))
