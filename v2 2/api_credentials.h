#ifndef API_CREDENTIALS_H
#define API_CREDENTIALS_H

#include <cstdlib>
#include <string>

// Use environment variables for security instead of hardcoded credentials
inline std::string get_client_id() {
    const char* id = std::getenv("DERIBIT_CLIENT_ID");
    return id ? id : "Qw5tFiQy";  // Fallback value
}

inline std::string get_client_secret() {
    const char* secret = std::getenv("DERIBIT_CLIENT_SECRET");
    return secret ? secret : "RHrbLV5gN29aTc-qXdI67ojOL2-tIbP0hKmAe5FupOo";  // Fallback value
}

#endif // API_CREDENTIALS_H