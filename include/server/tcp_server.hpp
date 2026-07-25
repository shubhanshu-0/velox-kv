#pragma once
#include "../core/cache_manager.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @file tcp_server.hpp
 * @brief VeloxKV TCP Server interface.
 *
 * Each client connection is handled in a separate thread.
 * All clients share a common thread-safe cache instance.
 *
 * Protocol: Simple text commands
 * Commands:
 *   - INIT <type> <capacity> (type: serial, concurrent, sharded)
 *   - GET key
 *   - SET key value [expiration_seconds]
 *   - DEL key
 *   - FLUSH
 *   - STATS
 *   - INFO
 *   - HELP
 *   - QUIT
 */

class VeloxKVServer
{
public:
    enum class CacheType
    {
        SERIAL = 0,
        CONCURRENT = 1,
        SHARDED = 2
    };

    /**
     * @brief Constructor for the TCP server.
     *
     * @param port The port number to listen on.
     */
    VeloxKVServer(uint16_t port);
    ~VeloxKVServer();

    /**
     * @brief Pre-initializes the cache with a specific type and capacity.
     */
    bool initialize(CacheType type, uint32_t capacity);

    /**
     * @brief Starts the server listening loop. Blocks the calling thread.
     */
    bool start();

    /**
     * @brief Stops the server listening loop and closes the socket.
     */
    void stop();

    /**
     * @brief Checks if the server is running.
     */
    bool is_running() const { return running; }

private:
    int server_socket;
    uint16_t port;
    std::atomic<bool> running;

    std::unique_ptr<Cache<std::string, std::string>> cache;
    CacheType cache_type;
    uint32_t capacity;
    bool cache_initialized;

    std::atomic<uint32_t> active_connections{0};

    /**
     * @brief Handles a client socket session in a dedicated thread.
     */
    void handle_client(int client_socket);

    /**
     * @brief Parses and executes a command string.
     */
    std::string process_command(const std::string &command);

    std::string handle_init(const std::vector<std::string> &args);
    std::string handle_get(const std::string &key);
    std::string handle_set(const std::string &args);
    std::string handle_del(const std::string &key);
    std::string handle_flush();
    std::string handle_stats();
    std::string handle_info();
    std::string handle_quit();
    std::string handle_help();

    std::vector<std::string> parse_command(const std::string &line);
};
