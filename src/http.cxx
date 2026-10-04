#include "http.hxx"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/file.hpp>
#include <charconv>
#include <filesystem>
#include <chrono>
#include <openssl/ssl.h>
#include <stdexcept>
#include <utility>
#include <string_view>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp = net::ip::tcp;
using namespace std::chrono_literals;

namespace {
    constexpr auto timeout = 30s;           // for each network operation

    auto default_port(const std::string& scheme) -> std::string {
        return scheme == "https" ? "443" : "80";
    }
}

auto parse_url(const std::string& url) -> Url {
    auto bad = [&](const std::string& why) { return std::runtime_error("invalid URL '" + url + "': " + why); };

    auto sep = url.find("://");
    if (sep == std::string::npos || sep == 0) throw bad("missing scheme");

    auto result = Url();
    result.scheme = url.substr(0, sep);
    if (result.scheme != "http" && result.scheme != "https") throw bad("scheme must be http or https");

    auto rest      = std::string_view(url).substr(sep + 3);
    auto path_pos  = rest.find_first_of("/?");
    auto authority = rest.substr(0, path_pos);
    if (path_pos == std::string_view::npos)  result.target = "/";
    else if (rest[path_pos] == '?')          result.target = "/" + std::string(rest.substr(path_pos));
    else                                     result.target = std::string(rest.substr(path_pos));

    auto colon = authority.rfind(':');
    result.host = std::string(authority.substr(0, colon));
    result.port = colon == std::string_view::npos ? default_port(result.scheme)
                                                  : std::string(authority.substr(colon + 1));
    if (result.host.empty()) throw bad("missing host");

    auto port = 0;
    auto [end, ec] = std::from_chars(result.port.data(), result.port.data() + result.port.size(), port);
    if (ec != std::errc{} || end != result.port.data() + result.port.size() || port < 1 || port > 65535) {
        throw bad("invalid port '" + result.port + "'");
    }
    return result;
}

auto archive_url(const std::string& base_url, int year) -> std::string {
    auto base = base_url;
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + "/" + std::to_string(year) + ".tar.gz";
}

struct HttpSource::Connection {
    net::io_context                       ioc;
    ssl::context                          tls{ssl::context::tls_client};
    beast::ssl_stream<beast::tcp_stream>  stream{ioc, tls};
    beast::flat_buffer                    buffer;       // holds what was read ahead of the body
    http::response_parser<http::buffer_body> parser;
    std::string                           url_text;     // for error messages

    explicit Connection(std::string text) : url_text(std::move(text)) {
        auto url = parse_url(url_text);
        if (url.scheme != "https") throw std::runtime_error("only https is supported: " + url_text);

        // Verify the server certificate against the system's certificate authorities.
        // The mode is set on the stream; the stream has already copied the context's mode.
        tls.set_default_verify_paths();
        stream.set_verify_mode(ssl::verify_peer);
        stream.set_verify_callback(ssl::host_name_verification(url.host));
        if (!SSL_set_tlsext_host_name(stream.native_handle(), url.host.c_str())) {      // SNI
            throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
        }

        auto& socket = beast::get_lowest_layer(stream);
        socket.expires_after(timeout);
        socket.connect(tcp::resolver(ioc).resolve(url.host, url.port));
        stream.handshake(ssl::stream_base::client);

        auto request = http::request<http::empty_body>(http::verb::get, url.target, 11);
        request.set(http::field::host, url.host);
        request.set(http::field::user_agent, "boost-demo");
        http::write(stream, request);

        parser.body_limit(boost::none);     // the default limit is 8 MB, the archives are bigger
        http::read_header(stream, buffer, parser);

        auto status = parser.get().result_int();
        if (status != 200) {
            throw std::runtime_error("HTTP " + std::to_string(status) + " " + std::string(parser.get().reason())
                                     + " for " + url_text);
        }
    }
};

// Boost.System's what() text includes the source location in Asio, the message alone reads better
HttpSource::HttpSource(const std::string& url) {
    try {
        conn_ = std::make_shared<Connection>(url);
    } catch (const boost::system::system_error& e) {
        throw std::runtime_error(url + ": " + e.code().message());
    }
}

auto HttpSource::read(char* buffer, std::streamsize n) -> std::streamsize {
    auto& c = *conn_;
    while (!c.parser.is_done()) {
        // The parser fills the caller's buffer directly; body().size is what is left of it
        auto& body = c.parser.get().body();
        body.data  = buffer;
        body.size  = static_cast<std::size_t>(n);

        beast::get_lowest_layer(c.stream).expires_after(timeout);
        auto ec = beast::error_code();
        http::read(c.stream, c.buffer, c.parser, ec);
        if (ec && ec != http::error::need_buffer) {                                // need_buffer = the buffer is full
            throw std::runtime_error(c.url_text + ": " + ec.message());
        }

        auto received = n - static_cast<std::streamsize>(body.size);
        if (received > 0) return received;
    }
    return -1;
}

auto download(const std::string& url, const std::filesystem::path& target) -> std::uintmax_t {
    namespace fs = std::filesystem;
    if (target.has_parent_path()) fs::create_directories(target.parent_path());

    auto partial = fs::path(target) += ".part";
    try {
        auto source = HttpSource(url);
        auto sink   = boost::iostreams::file_sink(partial.string(), std::ios::binary);
        boost::iostreams::copy(source, sink, 64 * 1024);
    } catch (const std::ios_base::failure&) {
        fs::remove(partial);
        throw std::runtime_error("cannot write " + partial.string());
    } catch (...) {
        fs::remove(partial);
        throw;
    }
    fs::rename(partial, target);
    return fs::file_size(target);
}
