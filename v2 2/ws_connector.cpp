#include "ws_connector.h"
#include <boost/asio/ip/tcp.hpp>
#include <iostream>

namespace ssl_alias = boost::asio::ssl;
namespace ip_alias = boost::asio::ip;
namespace beast_alias = boost::beast;

// Define the custom teardown function in the boost::beast namespace
namespace boost {
namespace beast {
void teardown(
    role_type role,
    ssl_alias::stream<ip_alias::tcp::socket>& stream,
    error_code& ec) {
    stream.shutdown(ec); // Gracefully shut down the SSL stream
}
} // namespace beast
} // namespace boost

WsConnector::WsConnector(const std::string& server, const std::string& port_num, const std::string& path)
    : server_(server),
      port_num_(port_num),
      path_(path),
      io_service_(),
      ssl_ctx_(ssl_alias::context::tlsv13_client),
      dns_resolver_(io_service_),
      ws_stream_(io_service_, ssl_ctx_),
      receive_buffer_(8192)  // Pre-allocated buffer to avoid heap fragmentation
{
    // Configure SSL context for security
    ssl_ctx_.set_options(ssl_alias::context::default_workarounds |
                         ssl_alias::context::no_sslv2 |
                         ssl_alias::context::no_sslv3 |
                         ssl_alias::context::single_dh_use);

    SSL_CTX_set_options(ssl_ctx_.native_handle(), SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_mode(ssl_ctx_.native_handle(), SSL_MODE_RELEASE_BUFFERS);
    SSL_CTX_set_session_cache_mode(ssl_ctx_.native_handle(), SSL_SESS_CACHE_CLIENT);

    const char* ciphers = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:ECDHE-ECDSA-AES256-GCM-SHA384";
    if (SSL_CTX_set_cipher_list(ssl_ctx_.native_handle(), ciphers) != 1) {
        std::cerr << "Failed to configure cipher suite" << std::endl;
    }
}

void WsConnector::establishConnection() {
    try {
        auto endpoints = dns_resolver_.resolve(server_, port_num_);
        boost::asio::connect(ws_stream_.next_layer().next_layer(), endpoints);
        
        // Disable Nagle's algorithm for lower latency
        ip_alias::tcp::no_delay no_delay(true);
        ws_stream_.next_layer().next_layer().set_option(no_delay);

        ws_stream_.next_layer().handshake(ssl_alias::stream_base::client);

        ws_stream_.set_option(beast_alias::websocket::stream_base::decorator(
            [](beast_alias::websocket::request_type& request) {
                request.set(beast_alias::http::field::user_agent, "CustomTradingApp");
            }));

        ws_stream_.handshake(server_, path_);
    } catch (const std::exception& ex) {
        std::cerr << "Connection attempt failed: " << ex.what() << std::endl;
        throw;
    }
}

void WsConnector::transmit(const std::string& data) {
    try {
        ws_stream_.write(boost::asio::buffer(data));
    } catch (const std::exception& ex) {
        std::cerr << "Data transmission failed: " << ex.what() << std::endl;
        throw;
    }
}

std::string WsConnector::receive() {
    try {
        beast_alias::flat_buffer buffer;
        ws_stream_.read(buffer);

        // Convert buffer to string using Boost.Beast utility
        return beast_alias::buffers_to_string(buffer.data());
    } catch (const std::exception& ex) {
        std::cerr << "Data reception failed: " << ex.what() << std::endl;
        throw;
    }
}

bool WsConnector::isConnected() const {
    return ws_stream_.is_open();
}

void WsConnector::disconnect() {
    try {
        beast_alias::error_code err;
        ws_stream_.close(beast_alias::websocket::close_code::normal, err);
        if (err) {
            std::cerr << "WebSocket shutdown error: " << err.message() << std::endl;
        }

        beast_alias::teardown(beast_alias::role_type::client, ws_stream_.next_layer(), err);
        if (err && err != boost::asio::error::eof) {
            std::cerr << "SSL shutdown error: " << err.message() << std::endl;
        }

        ws_stream_.next_layer().lowest_layer().close(err);
        if (err) {
            std::cerr << "Socket closure error: " << err.message() << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Disconnection error: " << ex.what() << std::endl;
    }
}
