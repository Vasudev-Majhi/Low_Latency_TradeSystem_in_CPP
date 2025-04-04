#include "order_manager.h"
#include <rapidjson/prettywriter.h>  // Include this at the top of the file

// Helper function to pretty-print JSON
std::string prettyPrintJson(const rapidjson::Document& doc) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

#include "ws_connector.h"
#include <iostream>
#include <stdexcept>
#include "performance_tracker.h"

alignas(64) std::atomic<int> OrderManager::sequence_num_{1};
thread_local rapidjson::Document OrderManager::json_cache_;

OrderManager::OrderManager(WsConnector& ws_conn) : ws_conn_(ws_conn) {}

OrderManager::~OrderManager() {
    feed_handlers_.clear();
}

int OrderManager::generateSequenceNum() {
    return sequence_num_.fetch_add(1, std::memory_order_relaxed);
}

void OrderManager::processMarketFeed(const rapidjson::Document& feed) {
    for (const auto& [asset, handler] : feed_handlers_) {
        if (feed.HasMember(asset.c_str())) {
            handler(feed);
        }
    }
}

void OrderManager::onFeedReceived(const rapidjson::Document& market_feed) {
    auto feed_start = PerformanceTracker::beginTiming();
    processMarketFeed(market_feed);
    PerformanceTracker::endTiming(feed_start, "Feed Processing Time");
}

rapidjson::Document OrderManager::performAuthentication(const std::string& id, const std::string& secret) {
    try {
        json_cache_.SetObject();
        auto& allocator = json_cache_.GetAllocator();

        json_cache_.AddMember("jsonrpc", "2.0", allocator);
        json_cache_.AddMember("method", "public/auth", allocator);

        rapidjson::Value params(rapidjson::kObjectType);
        params.AddMember("grant_type", "client_credentials", allocator);
        params.AddMember("client_id", rapidjson::Value(id.c_str(), allocator), allocator);
        params.AddMember("client_secret", rapidjson::Value(secret.c_str(), allocator), allocator);

        json_cache_.AddMember("params", params, allocator);
        json_cache_.AddMember("id", generateSequenceNum(), allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_cache_.Accept(writer);

        ws_conn_.transmit(buffer.GetString());
        std::string reply = ws_conn_.receive();

        rapidjson::Document result;
        result.Parse(reply.c_str());

        if (!result.HasMember("result")) {
            throw std::runtime_error("Auth error: " + reply);
        }

        // Store the access token for future requests
        access_token_ = result["result"]["access_token"].GetString();

        rapidjson::Document auth_result;
        auth_result.SetObject();
        auth_result.CopyFrom(result["result"], auth_result.GetAllocator());

        return auth_result;
    } catch (const std::exception& ex) {
        std::cerr << "Authentication issue: " << ex.what() << std::endl;
        throw;
    }
}

rapidjson::Document OrderManager::submitBuyOrder(const std::string& asset, double qty, double rate) {
    try {
        json_cache_.SetObject();
        auto& allocator = json_cache_.GetAllocator();

        json_cache_.AddMember("jsonrpc", "2.0", allocator);
        json_cache_.AddMember("method", "private/buy", allocator);
        json_cache_.AddMember("id", generateSequenceNum(), allocator);

        rapidjson::Value params(rapidjson::kObjectType);
        params.AddMember("instrument_name", rapidjson::Value(asset.c_str(), allocator), allocator);
        params.AddMember("amount", qty, allocator);
        params.AddMember("price", rate, allocator);
        params.AddMember("type", "limit", allocator);
        params.AddMember("post_only", true, allocator);
        params.AddMember("access_token", rapidjson::Value(access_token_.c_str(), allocator), allocator);  // Auth

        json_cache_.AddMember("params", params, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_cache_.Accept(writer);

        ws_conn_.transmit(buffer.GetString());
        std::string reply = ws_conn_.receive();

        rapidjson::Document result;
        result.Parse(reply.c_str());

        if (result.HasMember("error")) {
            throw std::runtime_error("Buy order error: " + std::string(buffer.GetString()));
        }

        return result;
    } catch (const std::exception& ex) {
        std::cerr << "Buy order error: " << ex.what() << std::endl;
        throw;
    }
}

rapidjson::Document OrderManager::removeOrder(const std::string& order_ref) {
    try {
        json_cache_.SetObject();
        auto& allocator = json_cache_.GetAllocator();

        json_cache_.AddMember("jsonrpc", "2.0", allocator);
        json_cache_.AddMember("method", "private/cancel", allocator);
        json_cache_.AddMember("id", generateSequenceNum(), allocator);

        rapidjson::Value params(rapidjson::kObjectType);
        params.AddMember("order_id", rapidjson::Value(order_ref.c_str(), allocator), allocator);
        params.AddMember("access_token", rapidjson::Value(access_token_.c_str(), allocator), allocator);  // Auth

        json_cache_.AddMember("params", params, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_cache_.Accept(writer);

        ws_conn_.transmit(buffer.GetString());
        std::string reply = ws_conn_.receive();

        rapidjson::Document result;
        result.Parse(reply.c_str());

        if (result.HasMember("error")) {
            throw std::runtime_error("Order removal error: " + std::string(buffer.GetString()));
        }

        return result;
    } catch (const std::exception& ex) {
        std::cerr << "Order removal error: " << ex.what() << std::endl;
        throw;
    }
}

rapidjson::Document OrderManager::updateOrder(const std::string& order_ref, double new_rate, double new_qty) {
    try {
        json_cache_.SetObject();
        auto& allocator = json_cache_.GetAllocator();

        json_cache_.AddMember("jsonrpc", "2.0", allocator);
        json_cache_.AddMember("method", "private/edit", allocator);
        json_cache_.AddMember("id", generateSequenceNum(), allocator);

        rapidjson::Value params(rapidjson::kObjectType);
        params.AddMember("order_id", rapidjson::Value(order_ref.c_str(), allocator), allocator);
        params.AddMember("price", new_rate, allocator);  // Corrected
        params.AddMember("amount", new_qty, allocator);  // Corrected
        params.AddMember("quantity", new_qty, allocator);  // Required for contracts
        params.AddMember("access_token", rapidjson::Value(access_token_.c_str(), allocator), allocator);  // Auth

        json_cache_.AddMember("params", params, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_cache_.Accept(writer);

        ws_conn_.transmit(buffer.GetString());
        std::string reply = ws_conn_.receive();

        rapidjson::Document result;
        result.Parse(reply.c_str());

        if (result.HasMember("error")) {
            throw std::runtime_error("Order update error: " + std::string(buffer.GetString()));
        }

        return result;
    } catch (const std::exception& ex) {
        std::cerr << "Order update error: " << ex.what() << std::endl;
        throw;
    }
}

 rapidjson::Document OrderManager::fetchPositions() {
    try {
        json_cache_.SetObject();
        auto& allocator = json_cache_.GetAllocator();

        json_cache_.AddMember("jsonrpc", "2.0", allocator);
        json_cache_.AddMember("method", "private/get_positions", allocator);
        json_cache_.AddMember("id", generateSequenceNum(), allocator);

        rapidjson::Value params(rapidjson::kObjectType);
        params.AddMember("currency", "BTC", allocator);  // Required
        params.AddMember("access_token", rapidjson::Value(access_token_.c_str(), allocator), allocator);  // Auth

        json_cache_.AddMember("params", params, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_cache_.Accept(writer);

        // Debug: Print the request payload
        std::cout << "Request Payload (fetchPositions): " << buffer.GetString() << std::endl;

        ws_conn_.transmit(buffer.GetString());
        std::string reply = ws_conn_.receive();

        // Debug: Print the API response
        std::cout << "API Response (fetchPositions): " << reply << std::endl;

        rapidjson::Document result;
        result.Parse(reply.c_str());

        if (result.HasMember("error")) {
            throw std::runtime_error("Position fetch error: " + std::string(buffer.GetString()));
        }

        // Pretty-print the result
        std::cout << "Position Details:\n" << prettyPrintJson(result) << std::endl;

        return result;
    } catch (const std::exception& ex) {
        std::cerr << "Position fetch error: " << ex.what() << std::endl;
        throw;
    }
}
rapidjson::Document OrderManager::retrieveOrderBook(const std::string& asset) {
    try {
        json_cache_.SetObject();
        auto& allocator = json_cache_.GetAllocator();

        json_cache_.AddMember("jsonrpc", "2.0", allocator);
        json_cache_.AddMember("method", "public/get_order_book", allocator);
        json_cache_.AddMember("id", generateSequenceNum(), allocator);

        rapidjson::Value params(rapidjson::kObjectType);
        params.AddMember("instrument_name", rapidjson::Value(asset.c_str(), allocator), allocator);

        json_cache_.AddMember("params", params, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_cache_.Accept(writer);

        // Debug: Print the request payload
        std::cout << "Request Payload (retrieveOrderBook): " << buffer.GetString() << std::endl;

        ws_conn_.transmit(buffer.GetString());
        std::string reply = ws_conn_.receive();

        // Debug: Print the API response
        std::cout << "API Response (retrieveOrderBook): " << reply << std::endl;

        rapidjson::Document result;
        result.Parse(reply.c_str());

        if (result.HasMember("error")) {
            throw std::runtime_error("Order book error: " + std::string(buffer.GetString()));
        }

        // Pretty-print the result
        std::cout << "Order Book Data:\n" << prettyPrintJson(result) << std::endl;

        return result;
    } catch (const std::exception& ex) {
        std::cerr << "Order book error: " << ex.what() << std::endl;
        throw;
    }
}


void OrderManager::registerMarketFeed(const std::string& asset, std::function<void(const rapidjson::Document&)> handler) {
    feed_handlers_.emplace(asset, std::move(handler));
}