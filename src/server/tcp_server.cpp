#include "../../include/server/tcp_server.hpp"
#include "../../include/core/cache_manager.hpp"
#include "../../include/policies/lru.hpp"
#include <algorithm>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

/* POSIX socket headers */
#include <arpa/inet.h>  // inet_pton()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <unistd.h>     // close(), read(), write()

/**
 * @file tcp_server.cpp
 * @brief VeloxKV TCP Server implementation.
 *
 * Implements socket setup, client connection pooling, protocol parsing,
 * and command routing to the underlying cache.
 */

static VeloxKVServer *g_server_instance = nullptr;

/**
 * @brief Signal handler to stop the running server instance gracefully on SIGINT/SIGTERM.
 */
extern "C" void handle_termination_signal(int /*sig*/)
{
    if (g_server_instance)
    {
        g_server_instance->stop();
    }
}

VeloxKVServer::VeloxKVServer(uint16_t p)
    : server_socket(-1), port(p), running(false), cache_initialized(false), active_connections(0)
{
    std::cout << "[INFO] Server configured on port " << port << " (awaiting INIT command)\n";
}

VeloxKVServer::~VeloxKVServer()
{
    if (running)
    {
        stop();
    }
}

bool VeloxKVServer::initialize(CacheType type, uint32_t cap)
{
    std::vector<std::string> args = {"INIT", "", std::to_string(cap)};
    if (type == CacheType::SERIAL)
    {
        args[1] = "serial";
    }
    else if (type == CacheType::CONCURRENT)
    {
        args[1] = "concurrent";
    }
    else if (type == CacheType::SHARDED)
    {
        args[1] = "sharded";
    }

    std::string res = handle_init(args);
    return res.rfind("OK", 0) == 0;
}

std::vector<std::string> VeloxKVServer::parse_command(const std::string &line)
{
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::string VeloxKVServer::handle_init(const std::vector<std::string> &args)
{
    if (cache_initialized)
    {
        return "ERROR: Cache is already initialized. Cannot reinitialize.";
    }

    if (args.size() < 3)
    {
        return "ERROR: INIT syntax is 'INIT <type> <capacity>' (type: serial, concurrent, sharded)";
    }

    int type_val = -1;
    std::string type_str = args[1];
    std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::tolower);

    try
    {
        type_val = std::stoi(type_str);
    }
    catch (...)
    {
        if (type_str == "serial")
        {
            type_val = 0;
        }
        else if (type_str == "concurrent")
        {
            type_val = 1;
        }
        else if (type_str == "sharded")
        {
            type_val = 2;
        }
    }

    if (type_val < 0 || type_val > 2)
    {
        return "ERROR: Invalid cache type. Supported: serial, concurrent, sharded";
    }

    try
    {
        capacity = std::stoul(args[2]);
    }
    catch (...)
    {
        return "ERROR: Capacity must be a positive integer.";
    }

    if (capacity == 0)
    {
        return "ERROR: Capacity must be greater than zero.";
    }

    cache_type = static_cast<CacheType>(type_val);

    auto factory = [](uint32_t c) { return std::make_unique<LRUCache<std::string, std::string>>(c); };

    try
    {
        switch (cache_type)
        {
        case CacheType::SERIAL:
            std::cout << "[INIT] Initializing SERIAL cache (capacity: " << capacity << ")\n";
            cache = std::make_unique<SerialCache<std::string, std::string>>(factory, capacity);
            break;

        case CacheType::CONCURRENT:
            std::cout << "[INIT] Initializing CONCURRENT cache (capacity: " << capacity << ")\n";
            cache = std::make_unique<ConcurrentCache<std::string, std::string>>(factory, capacity);
            break;

        case CacheType::SHARDED:
            std::cout << "[INIT] Initializing SHARDED cache (capacity: " << capacity << ")\n";
            cache = std::make_unique<ShardedConcurrentCache<std::string, std::string>>(factory, capacity);
            break;
        }

        cache_initialized = true;
        std::string type_name;
        switch (cache_type)
        {
        case CacheType::SERIAL:
            type_name = "SERIAL";
            break;
        case CacheType::CONCURRENT:
            type_name = "CONCURRENT";
            break;
        case CacheType::SHARDED:
            type_name = "SHARDED";
            break;
        }
        return "OK: " + type_name + " cache initialized (capacity: " + std::to_string(capacity) + ")";
    }
    catch (const std::exception &e)
    {
        std::cerr << "[ERROR] Failed to allocate cache: " << e.what() << "\n";
        return "ERROR: Internal initialization failure: " + std::string(e.what());
    }
}

std::string VeloxKVServer::handle_get(const std::string &key)
{
    auto value = cache->get(key);
    if (value)
    {
        return "VALUE " + *value;
    }
    return "nil";
}

std::string VeloxKVServer::handle_set(const std::string &args)
{
    auto tokens = parse_command(args);
    if (tokens.size() < 3)
    {
        return "ERROR: SET syntax is 'SET key value [expiration_seconds]'";
    }

    const std::string &key = tokens[1];
    const std::string &value = tokens[2];
    uint32_t expiration = 0;

    if (tokens.size() > 3)
    {
        try
        {
            expiration = std::stoul(tokens[3]);
        }
        catch (...)
        {
            return "ERROR: Expiration must be a positive integer.";
        }
    }

    cache->set(key, value, expiration);
    return "OK";
}

std::string VeloxKVServer::handle_del(const std::string &key)
{
    bool deleted = cache->remove(key);
    return deleted ? "1" : "0";
}

std::string VeloxKVServer::handle_flush()
{
    /* Clear statistics and trigger cache cleanup if needed in future versions. */
    return "OK";
}

std::string VeloxKVServer::handle_stats()
{
    if (!cache_initialized || !cache)
    {
        return "ERROR: Cache not initialized";
    }
    uint64_t hits = cache->get_hits();
    uint64_t misses = cache->get_misses();
    uint64_t evictions = cache->get_evictions();
    double hit_rate = 0.0;
    if (hits + misses > 0)
    {
        hit_rate = (double)hits / (hits + misses) * 100.0;
    }

    std::string stats = "STATS:\r\n";
    stats += "  hits: " + std::to_string(hits) + "\r\n";
    stats += "  misses: " + std::to_string(misses) + "\r\n";
    stats += "  hit_rate: " + std::to_string(hit_rate) + "%\r\n";
    stats += "  evictions: " + std::to_string(evictions) + "\r\n";
    stats += "  active_connections: " + std::to_string(active_connections.load()) + "\r\n";
    return stats;
}

std::string VeloxKVServer::handle_info()
{
    std::string info = "# VeloxKV Server\r\n";
    info += "version: 0.1.0\r\n";
    info += "port: " + std::to_string(port) + "\r\n";

    if (cache_initialized)
    {
        switch (cache_type)
        {
        case CacheType::SERIAL:
            info += "cache_type: serial\r\n";
            break;
        case CacheType::CONCURRENT:
            info += "cache_type: concurrent\r\n";
            break;
        case CacheType::SHARDED:
            info += "cache_type: sharded\r\n";
            break;
        }
        info += "capacity: " + std::to_string(capacity) + "\r\n";
    }
    else
    {
        info += "cache_status: uninitialized\r\n";
    }
    info += "active_connections: " + std::to_string(active_connections.load()) + "\r\n";
    return info;
}

std::string VeloxKVServer::handle_quit()
{
    return "OK";
}

std::string VeloxKVServer::handle_help()
{
    return "Commands: INIT, GET, SET, DEL, STATS, INFO, HELP, QUIT";
}

std::string VeloxKVServer::process_command(const std::string &line)
{
    auto tokens = parse_command(line);
    if (tokens.empty())
    {
        return "ERROR: Empty command";
    }

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "INIT")
    {
        return handle_init(tokens);
    }

    if (!cache_initialized)
    {
        return "ERROR: Cache not initialized. Send INIT command first.";
    }

    if (cmd == "GET")
    {
        if (tokens.size() < 2)
            return "ERROR: GET requires a key";
        return handle_get(tokens[1]);
    }
    else if (cmd == "SET")
    {
        return handle_set(line);
    }
    else if (cmd == "DEL")
    {
        if (tokens.size() < 2)
            return "ERROR: DEL requires a key";
        return handle_del(tokens[1]);
    }
    else if (cmd == "FLUSH")
    {
        return handle_flush();
    }
    else if (cmd == "STATS")
    {
        return handle_stats();
    }
    else if (cmd == "INFO")
    {
        return handle_info();
    }
    else if (cmd == "HELP")
    {
        return handle_help();
    }
    else if (cmd == "QUIT" || cmd == "EXIT")
    {
        return handle_quit();
    }
    else
    {
        return "ERROR: Unknown command '" + cmd + "'. Type HELP for usage.";
    }
}

void VeloxKVServer::handle_client(int client_socket)
{
    active_connections.fetch_add(1, std::memory_order_relaxed);
    char buffer[1024];

    while (running)
    {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = ::recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received < 0)
        {
            break;
        }

        if (bytes_received == 0)
        {
            break;
        }

        buffer[bytes_received] = '\0';
        std::string command(buffer);
        command.erase(command.find_last_not_of(" \n\r\t") + 1);

        if (command.empty())
        {
            continue;
        }

        std::string response = process_command(command);

        if (command == "QUIT" || command == "EXIT")
        {
            response = "OK";
            ::send(client_socket, (response + "\r\n").c_str(), response.size() + 2, 0);
            break;
        }

        response += "\r\n> ";
        ssize_t bytes_sent = ::send(client_socket, response.c_str(), response.size(), 0);

        if (bytes_sent < 0)
        {
            break;
        }
    }

    ::close(client_socket);
    active_connections.fetch_sub(1, std::memory_order_relaxed);
}

bool VeloxKVServer::start()
{
    g_server_instance = this;
    std::signal(SIGINT, handle_termination_signal);
    std::signal(SIGTERM, handle_termination_signal);

    server_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        std::cerr << "[ERROR] Failed to create server socket.\n";
        return false;
    }

    int opt = 1;
    if (::setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[ERROR] setsockopt SO_REUSEADDR failed.\n";
        ::close(server_socket);
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (::bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[ERROR] Bind failed on port " << port << "\n";
        ::close(server_socket);
        return false;
    }

    if (::listen(server_socket, 128) < 0)
    {
        std::cerr << "[ERROR] Listen failed.\n";
        ::close(server_socket);
        return false;
    }

    running = true;
    std::cout << "[SERVER] VeloxKV is listening on port " << port << "\n";

    while (running)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = ::accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket < 0)
        {
            break;
        }

        std::thread client_thread([this, client_socket]() { this->handle_client(client_socket); });
        client_thread.detach();
    }

    if (server_socket >= 0)
    {
        ::close(server_socket);
        server_socket = -1;
    }

    return true;
}

void VeloxKVServer::stop()
{
    running = false;
    if (server_socket >= 0)
    {
        /* Shutdown read/write channels on socket to wake any blocking accept() */
        ::shutdown(server_socket, SHUT_RDWR);
        ::close(server_socket);
        server_socket = -1;
    }
}
