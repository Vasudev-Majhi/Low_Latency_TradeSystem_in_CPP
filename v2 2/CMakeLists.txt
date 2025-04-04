cmake_minimum_required(VERSION 3.10)

# Define project and set C++ standard
project(HighFreqTradingApp)
set(CMAKE_CXX_STANDARD 17)

# Enable aggressive optimizations (-O3) and Link Time Optimization (LTO)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Add -O3 for aggressive optimizations
    add_compile_options(-O3)
    
    # Enable Link Time Optimization (LTO)
    add_compile_options(-flto)
    add_link_options(-flto)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # For MSVC, use /O2 (maximize speed) and /GL (Whole Program Optimization)
    add_compile_options(/O2 /GL)
    add_link_options(/LTCG)
endif()

# Path to Boost installation
set(BOOST_ROOT "C:/boost_1_87_0")

# Locate dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# Build executable with source files
add_executable(trading_client
    trading_client.cpp
    ws_connector.cpp
    order_manager.cpp
    performance_tracker.cpp
)

# Set output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

# Add include paths
target_include_directories(trading_client PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

# Link required libraries
target_link_libraries(trading_client PRIVATE
    ${Boost_LIBRARIES}
    OpenSSL::SSL
)