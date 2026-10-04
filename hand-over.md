# Hand-over: Boost article part 2 — the GSOD weather-data pipeline

**You are helping Jens Riboe write next week's article: a larger Boost example that
uses several components together.** It was planned as the final section of
*Kom igång med Boost* (`src/articles/2026/10-02_boost/index.md`), but that article
already reads at ~41 minutes, so the example was moved to its own article.
Work resumes **Thursday 2026-10-08** (Jens teaches C++ Templates Mon–Wed).

This document is context, not instructions. Read it, then read part 1 (`index.md`
in this folder) so part 2 does not repeat it.

---

## What part 1 already covers (do not repeat)

- What Boost is, history, Boost → std table, why Boost still matters.
- Installation: apt/dnf/brew/pacman, vcpkg, Conan, CMake FetchContent, building
  yourself into `~/Libs` (Jens's own setup, incl. the `install-cpp-lib` skill).
- `find_package(Boost CONFIG ...)`: module mode vs config mode, CMP0167.
- Header-only vs compiled libraries; static vs dynamic (Jens recommends static).
- Two small examples, in `boost-getting-started-demo/`:
  `moving-average` (Circular_buffer, header-only) and `banner` (Program_options,
  statically linked, shown with `ldd`).

Part 2 can assume the reader has Boost installed and knows `Boost::headers` vs
`Boost::<lib>`. Link back to part 1 rather than re-explaining.

Asio is reserved for **a separate, later article**. Part 2 may use Beast (which
sits on Asio) but should present the network code as "this is what it looks
like", deferring the *why* of Asio to that article.

---

## The program

A command-line tool that **downloads one year of NOAA weather data, decompresses
and unpacks it as a stream, extracts the measurements, aggregates them and prints
a readable report.** Nothing written to disk.

Working name: `gsod-stat` (not decided).

### Components, as agreed with Jens

| Step | Boost component | Notes |
|---|---|---|
| CLI: year, station, config file, … | **Program_options** | `--year`, `--station <id or name pattern>`, `--config`, maybe `--top N` |
| Download over HTTPS, streamed | **Beast** (+ Asio SSL, OpenSSL) | Jens's idea; he accepted the Asio overlap. Synchronous client. |
| Glue Beast → decompression | **Iostreams** | Custom `Source` device whose `read()` pulls the next body chunk from a Beast `response_parser` with `buffer_body` |
| gunzip | **Iostreams** `gzip_decompressor` | Jens's `libboost_iostreams.a` **is** built with zlib (verified) |
| Untar | *none in Boost* — write a small reader | See "tar" below; ~30 lines, a nice side-track for the article |
| Configuration | **Property_tree** | INI file: `unit = C\|F`, base URL, optional cache directory |
| Parse CSV lines | **String_algo** (and/or `boost::tokenizer` with `escaped_list_separator`) | Quoted fields containing commas — see "Format" |
| Aggregate | **Accumulators** | count, min, max, mean, variance/std-dev, median via `p_square_quantile` — per station and month |
| Present | `std::format` or Boost.Format | Per station: month table (min/mean/max) + text bar chart. Several stations: top list of hottest/coldest |

**Bigint (Multiprecision) was on Jens's original wish list but has no natural place
here** — it was raised with him and left open. Container (`flat_map` per station)
fits naturally.

### Pipeline

```
Beast (HTTPS GET, body read chunk by chunk)
  → custom Iostreams Source
  → gzip_decompressor            (filtering_istream)
  → tar reader                   (entry name + stream per station file)
  → line splitting / CSV parse   (String_algo / tokenizer)
  → accumulator_set per key      (Accumulators)
  → report
```

### Open decisions — ask Jens

1. **Beast or not.** Alternative: read `.tar.gz` from a file or stdin and let the
   user run `curl … | gsod-stat`. Simpler, no OpenSSL dependency — but the
   Beast→Iostreams coupling is the most interesting part. Jens leaned towards Beast.
2. Multiprecision: drop it, or find a genuine use.
3. Program name, and exact report layout.
4. Article title and date (presumably 2026-10-09 or later; future dates are
   supported — the article stays hidden in prod until its date).

---

## The data: NOAA GSOD (Global Summary of the Day)

Jens used this dataset ~2009 for Hadoop demos (it's the one made famous by Tom
White's *Hadoop: The Definitive Guide*). He did not know the agency had been
renamed — NCDC is now **NCEI**, and the old `ftp://ftp.ncdc.noaa.gov/pub/data/gsod/`
is gone.

| What | URL |
|---|---|
| One `.tar.gz` per year (1929–2025) | https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive/ |
| One CSV per station and year, uncompressed | https://www.ncei.noaa.gov/data/global-summary-of-the-day/access/1929/ etc. |
| ISD (more detailed, from 1901, fixed-width) | https://www.ncei.noaa.gov/pub/data/noaa/ |

- **HTTPS only**: plain HTTP answers `301` → HTTPS. Hence OpenSSL for Beast.
- **Updates appear to have stopped**: both GSOD and ISD last modified end of
  August 2025; no `2026` directory (`access/2026/` → 404). Not "data up to
  yesterday" any more — worth a sentence in the article.
- Sizes (compressed): 1929 = 48 KB (21 stations), 1950 = 12 MB, 1973 = 55 MB,
  2000 = 70 MB, 2024 = 106 MB. Use 1929 for quick tests.

### Format

Each tar entry is `<station-id>.csv`, e.g. `03005099999.csv`. All fields quoted:

```
"STATION","DATE","LATITUDE","LONGITUDE","ELEVATION","NAME","TEMP","TEMP_ATTRIBUTES","DEWP","DEWP_ATTRIBUTES","SLP","SLP_ATTRIBUTES","STP","STP_ATTRIBUTES","VISIB","VISIB_ATTRIBUTES","WDSP","WDSP_ATTRIBUTES","MXSPD","GUST","MAX","MAX_ATTRIBUTES","MIN","MIN_ATTRIBUTES","PRCP","PRCP_ATTRIBUTES","SNDP","FRSHTT"
"03005099999","1929-10-01","60.1333333","-1.1833333","84.0","LERWICK, UK","  45.3"," 4","  40.0"," 4","1001.6"," 4","999.9"," 0"," 17.1"," 4","  4.5"," 4","  8.9","999.9","  51.1"," ","  44.1","*"," 0.00","I","999.9","000000"
```

- Temperatures in **°F**, wind in knots, precipitation in inches.
- Missing values: `999.9` / `99.99` (and `9999.9` for pressures).
- `NAME` contains commas (`"LERWICK, UK"`) — a naive `split(",")` breaks.
- Numeric fields are space-padded inside the quotes → `trim`.
- `MAX`/`MIN` attribute `*` = derived from hourly data rather than reported.
- `FRSHTT` = six 0/1 flags: Fog, Rain, Snow, Hail, Thunder, Tornado.

### tar format (no Boost support)

512-byte header blocks: name at offset 0 (100 bytes, NUL-padded), size at offset
124 (12 bytes, **octal ASCII**), type flag at offset 156. Content follows, padded
to a multiple of 512. End of archive = two all-zero blocks. Check the archive for
ustar `prefix` (offset 345) or GNU long-name entries before assuming plain names.

---

## Toolchain and conventions

- GCC 16.1 at `/opt/gcc-16.1/bin/g++`; CMake 4.2.3 on PATH; Ninja.
- Boost 1.92.0 at `~/Libs/Boost/latest` — static, Release, built with CMake from
  the `-cmake` tarball. Use `-DBoost_ROOT=$HOME/Libs/Boost/latest`.
- OpenSSL and zlib headers are installed (`/usr/include/openssl/ssl.h`, `zlib.h`).
- Demo code lives in a `*-demo/` folder inside the article directory (excluded
  from the site build by `.eleventyignore`), published as a public GitHub repo
  with `scripts/publish-demo-repo.sh`. Part 1's repo:
  `ribomation/boost-getting-started-demo` — part 2 could get its own repo or a new
  folder in that one (ask).
- C++ style: see memory `feedback_cxx_demo_style.md` (AAA `auto`, trailing return
  type only for type names longer than four characters, English comments in code).
- **Every code snippet in the article is built and run for real**, and output in
  the article is copied from real runs.
- Article in Swedish; tables get `<div class="table-rows">` only when needed.
- Jens's workflow: plan and questions first → body → ingress → summary → proofread
  (present findings, he fixes small things himself).
