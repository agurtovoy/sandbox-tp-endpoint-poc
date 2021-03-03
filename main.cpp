#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json/src.hpp>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace json = boost::json;
using tcp = net::ip::tcp;

using url_t = std::tuple<std::string,std::string,std::string>;
inline std::string host(url_t const& url) { return std::get<0>(url); }
inline std::string port(url_t const& url) { return std::get<1>(url); }
inline std::string path(url_t const& url) { return std::get<2>(url); }

//------------------------------------------------------------------------------

void
fail(beast::error_code ec, char const* what)
{
    throw std::runtime_error(std::string(what) + ": " + ec.message());
}

class http_session : public std::enable_shared_from_this<http_session>
{
    using self_t = http_session;
    using put_callback_t = std::function<void (http::response<http::string_body>&)>;

    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    put_callback_t callback_;

public:

    // Objects are constructed with a strand to
    // ensure that handlers do not execute concurrently.
    explicit
    http_session(net::io_context& ioc)
        : resolver_(net::make_strand(ioc))
        , stream_(net::make_strand(ioc))
    {
    }

    // Initiate the asynchronous PUT request
    void
    put(
        url_t const& url,
        json::array const& input,
        put_callback_t const& callback
        )
    {
        req_.version(11);
        req_.method(http::verb::put);
        req_.target(path(url));
        req_.set(http::field::host, host(url));
        req_.set(http::field::content_type, "application/json");
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req_.body() = serialize(input);
        req_.prepare_payload();

        callback_ = callback;

        // Look up the domain name
        resolver_.async_resolve(
            host(url), port(url),
            beast::bind_front_handler(&self_t::on_resolve, shared_from_this())
        );
    }

    void
    on_resolve(
        beast::error_code ec,
        tcp::resolver::results_type results)
    {
        if (ec)
            return fail(ec, "resolve");

        // Set a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        stream_.async_connect(
            results,
            beast::bind_front_handler(&self_t::on_connect, shared_from_this())
        );
    }

    void
    on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if (ec)
            return fail(ec, "connect");

        // Set a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(
            stream_, req_,
            beast::bind_front_handler(&self_t::on_write, shared_from_this()
        ));
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");
        
        // Receive the HTTP response
        http::async_read(stream_, buffer_, res_,
            beast::bind_front_handler(&self_t::on_read, shared_from_this())
        );
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "read");

        if (callback_)
            callback_(res_);

        // Gracefully close the socket
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes so don't bother reporting it.
        if (ec && ec != beast::errc::not_connected)
            return fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

json::array make_image()
{
    json::array result;
    for (auto i = 0; i < 128; ++i) {
        json::array row;
        for (auto j = 0; j < 512; ++j) {
            row.emplace_back(0.0);
        }

        result.emplace_back(row);
    }

    return result;
}

void validate_result(json::array const& result)
{
    auto channels = result.at(0).as_array();

    if (channels.size() != 2)
        throw std::runtime_error("Expected an array of two channels, got an array of " + std::to_string(result.size()));

    for (auto& channel: channels) {
        auto array = channel.as_array();
        if (array.size() != 128 
            || (array.size() > 0 && !array.at(0).is_array()) 
            || (array.size() > 0 && array.at(0).as_array().size() != 512)
            ) {
            throw std::runtime_error("Expected a 128 x 512 channel, got " + std::to_string(array.size()) + " x ...");
        }
    }
}


int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: tp-endpoint-poc <hostname>\n";
        return EXIT_FAILURE;
    }

    auto const model_endpoint_url = url_t(argv[1], "8000", "/tpfinalpositiongan/v1");

    try {
        // The io_context is required for all I/O
        net::io_context ioc;
        
        json::array image1 = make_image();
        json::array image2 = make_image();

        json::array input;
        input.emplace_back(image1);
        input.emplace_back(image2);

        // Launch the asynchronous operation
        std::make_shared<http_session>(ioc)->put(model_endpoint_url, input, [](auto& res) {
            auto const status = res.result();
            if (status != http::status::ok)
                throw std::runtime_error("HTTP/" + std::to_string(static_cast<unsigned>(status)) + ": " + res.body());

            validate_result(json::parse(res.body()).as_array());
            std::cout << "Success!\n";
        });

        // Run the I/O service. The call will return when
        // the get operation is complete.
        ioc.run();
    } catch (std::exception const& x) {
        std::cerr << "ERROR: " << x.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
