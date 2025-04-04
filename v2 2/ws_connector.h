#ifndef WS_CONNECTOR_H
#define WS_CONNECTOR_H

#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

class alignas(64) WsConnector {
public:
    WsConnector(const std::string& server, const std::string& port_num, const std::string& path);
    void establishConnection();
    void transmit(const std::string& data);
    std::string receive();
    bool isConnected() const;
    void disconnect();

private:
    std::string server_;
    std::string port_num_;
    std::string path_;
    
    boost::asio::io_context io_service_;
    boost::asio::ssl::context ssl_ctx_;
    boost::asio::ip::tcp::resolver dns_resolver_;
    boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ws_stream_;

    std::vector<char> receive_buffer_;  // Pre-allocated buffer to avoid heap fragmentation
};

#endif // WS_CONNECTOR_H
