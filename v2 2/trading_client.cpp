#include "api_credentials.h"
#include "ws_connector.h"
#include "order_manager.h"
#include "performance_tracker.h"
#include <iostream>
#include <string>
#include <exception>
#include <memory>
#include <map>
#include <chrono>
#include <thread>
#include <rapidjson/document.h>

void runTradingOperations() {
    try {
        WsConnector ws_client("test.deribit.com", "443", "/ws/api/v2");

        std::cout << "Initiating WebSocket connection..." << std::endl;
        ws_client.establishConnection();

        int retry_count = 0;
        const int max_retries = 5;
        while (!ws_client.isConnected() && retry_count < max_retries) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            retry_count++;
            std::cout << "Retrying connection... (" << retry_count << "/" << max_retries << ")" << std::endl;
        }

        if (!ws_client.isConnected()) {
            std::cerr << "Connection failed after " << max_retries << " retries." << std::endl;
            return;
        }

        auto order_mgr = std::make_unique<OrderManager>(ws_client);
        rapidjson::Document auth_data = order_mgr->performAuthentication(get_client_id(), get_client_secret());

        if (auth_data.HasMember("error")) {
            std::cerr << "Authentication error.\n";
            return;
        }

        std::cout << "Authentication Successful.\n";

        std::unordered_map<std::string, rapidjson::Document> active_orders;

        while (true) {
            std::string asset_name, order_ref;
            double qty, rate;

            std::cout << "\n=== Trading Options ===\n";
            std::cout << "1. Create New Order\n2. Remove Order\n3. Update Order\n";
            std::cout << "4. Fetch Order Book\n5. Check Positions\n6. Quit\n";
            std::cout << "Select an option: ";
            int selection;
            std::cin >> selection;

            auto operation_start = PerformanceTracker::beginTiming();

            if (selection == 6) {
                std::cout << "Shutting down trading client.\n";
                break;
            }

            switch (selection) {
                case 1:
                    std::cout << "Asset name (e.g., BTC-PERPETUAL): ";
                    std::cin >> asset_name;
                    std::cout << "Quantity: ";
                    std::cin >> qty;
                    std::cout << "Rate: ";
                    std::cin >> rate;
                    try {
                        active_orders[asset_name] = order_mgr->submitBuyOrder(asset_name, qty, rate);
                        std::cout << "Order Submitted.\n";
                    } catch (const std::exception& ex) {
                        std::cerr << "Order submission failed: " << ex.what() << std::endl;
                    }
                    break;

                case 2:
                    std::cout << "Order reference to cancel: ";
                    std::cin >> order_ref;
                    try {
                        rapidjson::Document cancel_result = order_mgr->removeOrder(order_ref);
                        std::cout << "Cancellation Successful.\n";
                    } catch (const std::exception& ex) {
                        std::cerr << "Cancellation failed: " << ex.what() << std::endl;
                    }
                    break;

                case 3:
                    std::cout << "Order reference to update: ";
                    std::cin >> order_ref;
                    std::cout << "New rate: ";
                    std::cin >> rate;
                    std::cout << "New quantity: ";
                    std::cin >> qty;
                    try {
                        rapidjson::Document update_result = order_mgr->updateOrder(order_ref, rate, qty);
                        std::cout << "Update Successful.\n";
                    } catch (const std::exception& ex) {
                        std::cerr << "Update failed: " << ex.what() << std::endl;
                    }
                    break;

                case 4:
                    std::cout << "Asset name (e.g., BTC-PERPETUAL): ";
                    std::cin >> asset_name;
                    try {
                        rapidjson::Document book_data = order_mgr->retrieveOrderBook(asset_name);
                        std::cout << "Order Book Retrieved.\n";
                    } catch (const std::exception& ex) {
                        std::cerr << "Order book retrieval failed: " << ex.what() << std::endl;
                    }
                    break;

                case 5:
                    try {
                        rapidjson::Document position_data = order_mgr->fetchPositions();
                        std::cout << "Positions Retrieved.\n";
                    } catch (const std::exception& ex) {
                        std::cerr << "Position fetch failed: " << ex.what() << std::endl;
                    }
                    break;

                default:
                    std::cout << "Invalid option selected.\n";
                    break;
            }
            PerformanceTracker::endTiming(operation_start, "Trading Operation Duration");
        }

        ws_client.disconnect();
    } catch (const std::exception& ex) {
        std::cerr << "Trading operation error: " << ex.what() << std::endl;
    }
}

int main() {
    try {
        runTradingOperations();
    } catch (const std::exception& ex) {
        std::cerr << "Critical failure: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
