#ifndef ORDER_MANAGER_H
#define ORDER_MANAGER_H

#include <atomic>
#include <string>
#include <functional>
#include <unordered_map>
#include <boost/container/flat_map.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

class WsConnector;

class OrderManager {
public:
    explicit OrderManager(WsConnector& ws_conn);
    ~OrderManager();

    rapidjson::Document performAuthentication(const std::string& id, const std::string& secret);
    rapidjson::Document retrieveInstruments(const std::string& curr, const std::string& type, bool is_expired);
    rapidjson::Document submitBuyOrder(const std::string& asset, double qty, double rate);
    rapidjson::Document removeOrder(const std::string& order_ref);
    rapidjson::Document updateOrder(const std::string& order_ref, double new_rate, double new_qty);
    rapidjson::Document retrieveOrderBook(const std::string& asset);
    rapidjson::Document fetchPositions();

    void registerMarketFeed(const std::string& asset, std::function<void(const rapidjson::Document&)> handler);

private:
    std::string access_token_;
     int generateSequenceNum();
     
    void processMarketFeed(const rapidjson::Document& feed);
    void onFeedReceived(const rapidjson::Document& market_feed);

    WsConnector& ws_conn_;
    thread_local static rapidjson::Document json_cache_;

    std::unordered_map<std::string, std::function<void(const rapidjson::Document&)>> feed_handlers_;

    static std::atomic<int> sequence_num_;  // Removed alignas(64) from here
};

#endif // ORDER_MANAGER_H
