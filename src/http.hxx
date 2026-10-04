#pragma once
#include <boost/iostreams/concepts.hpp>
#include <cstdint>
#include <filesystem>
#include <ios>
#include <memory>
#include <string>

struct Url {
    std::string scheme;     // http or https
    std::string host;
    std::string port;       // the default port of the scheme, unless given
    std::string target;     // path and query, at least "/"
};

// Splits scheme://host[:port][/path][?query]. IPv6 addresses are not supported.
// Throws std::runtime_error if the URL is malformed.
auto parse_url(const std::string& url) -> Url;

// The URL of the archive for a year: <base_url>/<year>.tar.gz
auto archive_url(const std::string& base_url, int year) -> std::string;

// Downloads a URL to a file, streaming the body to disk. The data goes to
// <target>.part first, so a failed download never leaves a broken target file.
// Missing directories are created. Returns the size of the file in bytes.
auto download(const std::string& url, const std::filesystem::path& target) -> std::uintmax_t;

// An Iostreams Source that downloads a URL over HTTPS, using Beast. The
// response body is delivered a chunk at a time by read(), as the caller
// asks for it, and is never stored as a whole.
//
// Iostreams copies a device when it is pushed onto a stream, so the
// connection lives behind a shared_ptr.
class HttpSource : public boost::iostreams::source {
    struct Connection;                      // the Asio and Beast parts, see http.cxx
    std::shared_ptr<Connection> conn_;

public:
    // Connects, sends the request and reads the response header. Throws
    // unless the answer is 200 OK.
    explicit HttpSource(const std::string& url);

    // Reads up to n bytes of the body. Returns -1 at the end of the body.
    auto read(char* buffer, std::streamsize n) -> std::streamsize;
};
