#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <istream>
#include <streambuf>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

// A minimal reader for ustar archives. Boost has no tar support, but the
// format is simple: a sequence of 512-byte blocks, where each entry is a
// header block followed by the content, padded to a multiple of 512 bytes.
// The archive ends with (at least) two all-zero blocks.

struct TarEntry {
    std::string name;
    std::size_t size = 0;
    char        type = '0';     // '0' or NUL = regular file, '5' = directory

    auto is_file() const -> bool { return type == '0' || type == '\0'; }
};

class TarReader {
    static constexpr auto block_size = std::size_t{512};
    using Block = std::array<char, block_size>;

    // Exposes the content of the current entry as a stream that ends where the
    // entry ends, and keeps count of how much has been taken from the archive.
    class ContentBuf : public std::streambuf {
        std::istream&           in_;
        std::size_t&            unread_;
        std::array<char, 4096>  buf_{};

        auto underflow() -> int_type override {
            if (unread_ == 0) return traits_type::eof();
            in_.read(buf_.data(), static_cast<std::streamsize>(std::min(buf_.size(), unread_)));
            auto n = static_cast<std::size_t>(in_.gcount());
            if (n == 0) return traits_type::eof();
            unread_ -= n;
            setg(buf_.data(), buf_.data(), buf_.data() + n);
            return traits_type::to_int_type(buf_[0]);
        }
    public:
        ContentBuf(std::istream& in, std::size_t& unread) : in_(in), unread_(unread) {}
        void reset() { setg(nullptr, nullptr, nullptr); }
    };

    std::istream& in_;
    std::size_t   unread_  = 0;     // bytes of the current entry's content not yet taken from in_
    std::size_t   padding_ = 0;     // padding after the current entry's content
    ContentBuf    content_buf_{in_, unread_};
    std::istream  content_{&content_buf_};

    // A NUL-padded text field
    static auto text(const Block& b, std::size_t offset, std::size_t len) -> std::string {
        auto field = std::string_view(b.data() + offset, len);
        return std::string(field.substr(0, field.find('\0')));
    }

    // Numbers are octal ASCII, terminated by NUL or space
    static auto octal(const Block& b, std::size_t offset, std::size_t len) -> std::size_t {
        auto field = text(b, offset, len);
        field.erase(field.find_last_not_of(' ') + 1);
        auto value = std::size_t{0};
        auto [end, ec] = std::from_chars(field.data(), field.data() + field.size(), value, 8);
        if (ec != std::errc{} || end != field.data() + field.size()) {
            throw std::runtime_error("tar: invalid size field '" + field + "'");
        }
        return value;
    }

    void skip(std::size_t n) {
        in_.ignore(static_cast<std::streamsize>(n));    // works on non-seekable streams
        if (in_.gcount() != static_cast<std::streamsize>(n)) {
            throw std::runtime_error("tar: unexpected end of archive");
        }
    }

public:
    explicit TarReader(std::istream& in) : in_(in) {}
    TarReader(const TarReader&) = delete;
    auto operator=(const TarReader&) -> TarReader& = delete;

    // The content of the entry most recently returned by next(). Reading
    // past the end of the entry gives EOF.
    auto content() -> std::istream& { return content_; }

    // Moves on to the next entry. Whatever was not read of the previous
    // entry's content is skipped.
    auto next() -> std::optional<TarEntry> {
        content_buf_.reset();
        content_.clear();
        skip(unread_ + padding_);
        unread_ = padding_ = 0;

        auto header = Block{};
        if (!in_.read(header.data(), block_size)) {
            throw std::runtime_error("tar: unexpected end of archive");
        }
        if (std::ranges::all_of(header, [](char c) { return c == '\0'; })) {
            return std::nullopt;
        }

        auto entry = TarEntry{
            .name = text(header, 0, 100),
            .size = octal(header, 124, 12),
            .type = header[156],
        };
        if (entry.type == 'L' || entry.type == 'x' || entry.type == 'g') {
            throw std::runtime_error("tar: GNU long-name / pax extension headers are not supported");
        }
        if (text(header, 257, 5) == "ustar") {                 // ustar adds a path prefix
            if (auto prefix = text(header, 345, 155); !prefix.empty()) {
                entry.name = prefix + "/" + entry.name;
            }
        }

        unread_  = entry.size;
        padding_ = (block_size - entry.size % block_size) % block_size;
        return entry;
    }
};
