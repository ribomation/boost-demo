#include "tar.hxx"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <format>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace io = boost::iostreams;

namespace {
    constexpr auto block = std::size_t{512};

    // A 512-byte ustar header block
    auto header(const std::string& name, std::size_t size, char type = '0',
                const std::string& prefix = "") -> std::string {
        auto h = std::string(block, '\0');
        h.replace(0, name.size(), name);
        auto octal = std::format("{:011o}", size);
        h.replace(124, octal.size(), octal);
        h[156] = type;
        h.replace(257, 5, "ustar");
        h.replace(345, prefix.size(), prefix);
        return h;
    }

    // Header + content, padded to a multiple of 512 bytes
    auto file(const std::string& name, const std::string& content, const std::string& prefix = "") -> std::string {
        auto padding = (block - content.size() % block) % block;
        return header(name, content.size(), '0', prefix) + content + std::string(padding, '\0');
    }

    auto end_of_archive() -> std::string { return std::string(2 * block, '\0'); }
}

TEST_CASE("tar: empty archive", "[tar]") {
    auto in  = std::istringstream(end_of_archive());
    auto tar = TarReader(in);
    CHECK_FALSE(tar.next().has_value());
}

TEST_CASE("tar: entries and their content", "[tar]") {
    auto in = std::istringstream(file("a.csv", "hello") + file("b.csv", "world!!") + end_of_archive());
    auto tar = TarReader(in);

    auto a = tar.next();
    REQUIRE(a.has_value());
    CHECK(a->name == "a.csv");
    CHECK(a->size == 5);
    CHECK(a->type == '0');
    auto content = std::string(a->size, ' ');
    tar.content().read(content.data(), static_cast<std::streamsize>(content.size()));
    CHECK(content == "hello");

    auto b = tar.next();
    REQUIRE(b.has_value());
    CHECK(b->name == "b.csv");
    CHECK(b->size == 7);
    content.assign(b->size, ' ');
    tar.content().read(content.data(), static_cast<std::streamsize>(content.size()));
    CHECK(content == "world!!");

    CHECK_FALSE(tar.next().has_value());
}

TEST_CASE("tar: unread content and padding are skipped", "[tar]") {
    auto in = std::istringstream(file("a.csv", std::string(1000, 'x')) + file("b.csv", "y") + end_of_archive());
    auto tar = TarReader(in);

    REQUIRE(tar.next()->size == 1000);          // content of a.csv not read at all
    auto b = tar.next();
    REQUIRE(b.has_value());
    CHECK(b->name == "b.csv");
    CHECK(tar.content().get() == 'y');
}

TEST_CASE("tar: partly read content", "[tar]") {
    auto in = std::istringstream(file("a.csv", "line1\nline2\nline3\n") + file("b.csv", "next") + end_of_archive());
    auto tar = TarReader(in);

    REQUIRE(tar.next().has_value());
    auto line = std::string();
    std::getline(tar.content(), line);
    CHECK(line == "line1");

    REQUIRE(tar.next()->name == "b.csv");
    std::getline(tar.content(), line);
    CHECK(line == "next");
}

TEST_CASE("tar: content stream ends at the end of the entry", "[tar]") {
    auto in = std::istringstream(file("a.csv", "one\ntwo\n") + file("b.csv", "other") + end_of_archive());
    auto tar = TarReader(in);

    REQUIRE(tar.next().has_value());
    auto lines = std::vector<std::string>();
    for (auto line = std::string(); std::getline(tar.content(), line); ) lines.push_back(line);
    CHECK(lines == std::vector<std::string>{"one", "two"});
    CHECK(tar.content().eof());
}

TEST_CASE("tar: content larger than the internal buffer", "[tar]") {
    auto big = std::string();
    for (auto i = 0; i < 10000; ++i) big += static_cast<char>('a' + i % 26);
    auto in = std::istringstream(file("big", big) + file("after", "z") + end_of_archive());
    auto tar = TarReader(in);

    REQUIRE(tar.next()->size == big.size());
    auto got = std::string(std::istreambuf_iterator<char>(tar.content()), {});
    CHECK(got == big);
    CHECK(tar.next()->name == "after");
}

TEST_CASE("tar: content size at the block boundaries", "[tar]") {
    for (auto size : {std::size_t{0}, std::size_t{1}, std::size_t{511}, std::size_t{512}, std::size_t{513}}) {
        CAPTURE(size);
        auto in = std::istringstream(file("a", std::string(size, 'x')) + file("b", "") + end_of_archive());
        auto tar = TarReader(in);
        REQUIRE(tar.next()->size == size);
        REQUIRE(tar.next()->name == "b");
        REQUIRE_FALSE(tar.next().has_value());
    }
}

TEST_CASE("tar: entry types", "[tar]") {
    CHECK(TarEntry{"", 0, '0'}.is_file());
    CHECK(TarEntry{"", 0, '\0'}.is_file());
    CHECK_FALSE(TarEntry{"", 0, '5'}.is_file());       // directory
    CHECK_FALSE(TarEntry{"", 0, '2'}.is_file());       // symbolic link
}

TEST_CASE("tar: ustar prefix is prepended to the name", "[tar]") {
    auto in = std::istringstream(file("x.csv", "", "some/dir") + end_of_archive());
    auto tar = TarReader(in);
    CHECK(tar.next()->name == "some/dir/x.csv");
}

TEST_CASE("tar: size field may end with a space instead of NUL", "[tar]") {
    auto h = header("a", 0);
    h.replace(124, 12, "00000000012 ");        // 10 octal = 8 bytes
    auto in = std::istringstream(h + std::string(block, 'x') + end_of_archive());
    auto tar = TarReader(in);
    CHECK(tar.next()->size == 10);
}

TEST_CASE("tar: broken archives", "[tar]") {
    SECTION("no end marker") {
        auto in = std::istringstream(file("a", "x"));
        auto tar = TarReader(in);
        REQUIRE(tar.next().has_value());
        CHECK_THROWS_WITH(tar.next(), "tar: unexpected end of archive");
    }
    SECTION("content cut short") {
        auto in = std::istringstream(header("a", 2000) + std::string(100, 'x'));
        auto tar = TarReader(in);
        REQUIRE(tar.next().has_value());
        CHECK_THROWS_WITH(tar.next(), "tar: unexpected end of archive");
    }
    SECTION("header cut short") {
        auto in = std::istringstream(header("a", 0).substr(0, 100));
        auto tar = TarReader(in);
        CHECK_THROWS_WITH(tar.next(), "tar: unexpected end of archive");
    }
    SECTION("size is not an octal number") {
        auto h = header("a", 0);
        h.replace(124, 12, "12x45678901\0", 12);
        auto in = std::istringstream(h + end_of_archive());
        auto tar = TarReader(in);
        CHECK_THROWS_AS(tar.next(), std::runtime_error);
    }
}

TEST_CASE("tar: unsupported extension headers", "[tar]") {
    for (auto type : {'L', 'x', 'g'}) {
        CAPTURE(type);
        auto in = std::istringstream(header("././@LongLink", 0, type) + end_of_archive());
        auto tar = TarReader(in);
        CHECK_THROWS_AS(tar.next(), std::runtime_error);
    }
}

TEST_CASE("tar: through a gzip decompressor", "[tar]") {
    auto plain = file("one.csv", "first") + file("two.csv", std::string(700, 'z')) + end_of_archive();

    auto compressed = std::ostringstream();
    {
        auto out = io::filtering_ostream();
        out.push(io::gzip_compressor());
        out.push(compressed);
        out << plain;
    }                                           // closing the filter flushes the gzip trailer

    auto file_in = std::istringstream(compressed.str());
    auto in = io::filtering_istream();
    in.push(io::gzip_decompressor());
    in.push(file_in);

    auto tar = TarReader(in);
    REQUIRE(tar.next()->name == "one.csv");
    auto two = tar.next();
    REQUIRE(two.has_value());
    CHECK(two->name == "two.csv");
    CHECK(two->size == 700);
    CHECK_FALSE(tar.next().has_value());
}

TEST_CASE("tar: gzip errors are not swallowed by the stream", "[tar]") {
    auto garbage = std::istringstream(std::string(2000, 'x'));
    auto in = io::filtering_istream();
    in.push(io::gzip_decompressor());
    in.push(garbage);
    in.exceptions(std::ios::badbit);        // without this, the stream just goes bad silently

    auto tar = TarReader(in);
    try {
        tar.next();
        FAIL("expected a gzip_error");
    } catch (const io::gzip_error& e) {
        CHECK(e.error() == io::gzip::bad_header);
    }
}
