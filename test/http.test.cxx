#include "http.hxx"
#include "tar.hxx"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

namespace io = boost::iostreams;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("url: all parts", "[http]") {
    auto url = parse_url("https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive/1929.tar.gz");
    CHECK(url.scheme == "https");
    CHECK(url.host == "www.ncei.noaa.gov");
    CHECK(url.port == "443");
    CHECK(url.target == "/data/global-summary-of-the-day/archive/1929.tar.gz");
}

TEST_CASE("url: explicit port", "[http]") {
    auto url = parse_url("https://localhost:8443/x/y");
    CHECK(url.host == "localhost");
    CHECK(url.port == "8443");
    CHECK(url.target == "/x/y");
}

TEST_CASE("url: default port depends on the scheme", "[http]") {
    CHECK(parse_url("http://example.com/").port == "80");
    CHECK(parse_url("https://example.com/").port == "443");
}

TEST_CASE("url: path and query", "[http]") {
    CHECK(parse_url("https://example.com").target == "/");
    CHECK(parse_url("https://example.com/").target == "/");
    CHECK(parse_url("https://example.com?a=1").target == "/?a=1");
    CHECK(parse_url("https://example.com/p?a=1&b=2").target == "/p?a=1&b=2");
}

TEST_CASE("url: malformed", "[http]") {
    CHECK_THROWS_WITH(parse_url("example.com/x"), ContainsSubstring("missing scheme"));
    CHECK_THROWS_WITH(parse_url("://example.com"), ContainsSubstring("missing scheme"));
    CHECK_THROWS_WITH(parse_url("ftp://example.com/x"), ContainsSubstring("scheme must be http or https"));
    CHECK_THROWS_WITH(parse_url("https:///x"), ContainsSubstring("missing host"));
    CHECK_THROWS_WITH(parse_url("https://example.com:/x"), ContainsSubstring("invalid port"));
    CHECK_THROWS_WITH(parse_url("https://example.com:abc/x"), ContainsSubstring("invalid port"));
    CHECK_THROWS_WITH(parse_url("https://example.com:70000/x"), ContainsSubstring("invalid port"));
    CHECK_THROWS_WITH(parse_url("https://example.com:0/x"), ContainsSubstring("invalid port"));
}

TEST_CASE("url: archive url", "[http]") {
    CHECK(archive_url("https://host/archive", 1929) == "https://host/archive/1929.tar.gz");
    CHECK(archive_url("https://host/archive/", 1960) == "https://host/archive/1960.tar.gz");
    CHECK(archive_url("https://host/archive//", 2000) == "https://host/archive/2000.tar.gz");
}

TEST_CASE("http source: plain http is refused", "[http]") {
    CHECK_THROWS_WITH(HttpSource("http://example.com/"), ContainsSubstring("only https is supported"));
}

// These need internet access, so they are hidden. Run them with: unit-tests "[network]"
TEST_CASE("http source: download and unpack a real archive", "[.network]") {
    auto source = HttpSource("https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive/1929.tar.gz");
    auto in = io::filtering_istream();
    in.push(io::gzip_decompressor());
    in.push(source, 64 * 1024);
    in.exceptions(std::ios::badbit);

    auto tar   = TarReader(in);
    auto count = 0;
    while (tar.next()) ++count;
    CHECK(count == 21);
}

TEST_CASE("http source: not found", "[.network]") {
    CHECK_THROWS_WITH(HttpSource("https://www.ncei.noaa.gov/data/global-summary-of-the-day/access/2026/"),
                      ContainsSubstring("HTTP 404"));
}

TEST_CASE("http source: unknown host", "[.network]") {
    CHECK_THROWS_WITH(HttpSource("https://no-such-host.invalid/x"), ContainsSubstring("Host not found"));
}

TEST_CASE("http source: connection refused", "[.network]") {
    CHECK_THROWS_WITH(HttpSource("https://localhost:9/x"), ContainsSubstring("Connection refused"));
}

// badssl.com has servers with deliberately broken certificates
TEST_CASE("http source: certificates are verified", "[.network]") {
    CHECK_THROWS_WITH(HttpSource("https://expired.badssl.com/"),     ContainsSubstring("certificate verify failed"));
    CHECK_THROWS_WITH(HttpSource("https://wrong.host.badssl.com/"),  ContainsSubstring("certificate verify failed"));
    CHECK_THROWS_WITH(HttpSource("https://self-signed.badssl.com/"), ContainsSubstring("certificate verify failed"));
    CHECK_NOTHROW(HttpSource("https://badssl.com/"));       // a valid certificate, as a control
}

TEST_CASE("download: saves the archive, creating directories", "[.network]") {
    auto dir    = std::filesystem::temp_directory_path() / "boost-demo-test-download";
    auto target = dir / "sub" / "1929.tar.gz";
    std::filesystem::remove_all(dir);

    auto bytes = download("https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive/1929.tar.gz", target);
    CHECK(bytes == 48705);
    CHECK(std::filesystem::file_size(target) == bytes);
    CHECK_FALSE(std::filesystem::exists(target.string() + ".part"));
    std::filesystem::remove_all(dir);
}

TEST_CASE("download: a failed download leaves nothing behind", "[.network]") {
    auto dir    = std::filesystem::temp_directory_path() / "boost-demo-test-download-fail";
    auto target = dir / "2026.tar.gz";
    std::filesystem::remove_all(dir);

    CHECK_THROWS_WITH(download("https://www.ncei.noaa.gov/data/global-summary-of-the-day/archive/2026.tar.gz", target),
                      ContainsSubstring("HTTP 404"));
    CHECK_FALSE(std::filesystem::exists(target));
    CHECK_FALSE(std::filesystem::exists(target.string() + ".part"));
    std::filesystem::remove_all(dir);
}
