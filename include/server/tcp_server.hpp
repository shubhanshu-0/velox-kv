#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "../core/cache_manager.hpp"

/*
    VeloxKV TCP Server
    
    Listens on port 6380 for client connections.
    Each client connection is handled in a separate thread.
    All clients share the same cache instance (thread-safe via shared_mutex).
    
    Protocol: Simple text commands
    Commands:
      - GET key
      - SET key value [expiration_seconds]
      - DEL key
      - FLUSH
      - STATS
      - INFO
      - QUIT
*/

class VeloxKVServer {
public:
    enum class CacheType {
        SERIAL = 0,
        CONCURRENT = 1,
        SHARDED = 2
    };

    // Constructor: takes only port (cache initialized by client via INIT command)
    VeloxKVServer(uint16_t port);
    
    // Destructor: cleanup
    ~VeloxKVServer();
    
    // Start server (blocks, listens for clients)
    bool start();
    
    // Stop server gracefully
    void stop();
    
    // Check if server is running
    bool is_running() const { return running; }

private:
    // Socket and server state
    int server_socket;
    uint16_t port;
    bool running;
    
    // Cache instance (shared by all clients, nullptr until INIT command)
    std::unique_ptr<Cache<std::string, std::string>> cache;
    CacheType cache_type;
    uint32_t capacity;
    bool cache_initialized;
    
    // Handle individual client connection (runs in separate thread)
    // Takes the client's socket file descriptor
    void handle_client(int client_socket);
    
    // Process a single command from client
    // Returns response string to send back to client
    std::string process_command(const std::string& command);
    
    // Command handlers
    std::string handle_init(const std::vector<std::string>& args);
    std::string handle_get(const std::string& key);
    std::string handle_set(const std::string& args);
    std::string handle_del(const std::string& key);
    std::string handle_flush();
    std::string handle_stats();
    std::string handle_info();
    std::string handle_quit();
    std::string handle_help();
    
    // Utility: parse command line
    std::vector<std::string> parse_command(const std::string& line);
};
