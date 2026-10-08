#pragma once
#include "../policies/lru.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <poll.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file tcp_server.hpp
 * @brief VeloxKV TCP Server interface (Single-threaded Non-blocking Event Loop).
 *
 * All client connections are managed asynchronously in a single non-blocking event loop using poll().
 * All clients share a common cache instance.
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

struct ClientSession
{
    int socket_fd;
    std::string read_buffer;
};

class VeloxKVServer
{
public:
    /**
     * @brief Constructor for the TCP server.
     *
     * @param port The port number to listen on.
     * @param capacity Maximum capacity for the cache.
     */
    VeloxKVServer(uint16_t port, uint32_t capacity = 10000);
    ~VeloxKVServer();

    /**
     * @brief Initializes the cache engine.
     */
    bool initialize(uint32_t capacity);

    /**
     * @brief Starts the server event loop. Blocks the calling thread.
     */
    bool start();

    /**
     * @brief Stops the server event loop and closes all client sockets.
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
    uint32_t capacity;
    bool cache_initialized;

    std::atomic<uint32_t> active_connections{0};

    std::vector<struct pollfd> poll_fds;
    std::unordered_map<int, ClientSession> client_sessions;

    void set_nonblocking(int fd);
    void accept_new_client();
    void handle_client_data(int client_fd);
    void close_client(int client_fd);

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
