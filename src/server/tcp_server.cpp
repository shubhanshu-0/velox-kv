#include "../../include/server/tcp_server.hpp"
#include "../../include/policies/lru.hpp"
#include <algorithm>
#include <csignal>
#include <cstring>
#include <fcntl.h>
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
 * @brief VeloxKV TCP Server implementation (Single-threaded Non-blocking Event Loop).
 *
 * Implements non-blocking socket setup, poll() event multiplexing, line-buffered command parsing,
 * and command routing to the underlying cache engine.
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

VeloxKVServer::VeloxKVServer(uint16_t p, uint32_t cap)
    : server_socket(-1), port(p), running(false), capacity(cap), cache_initialized(false), active_connections(0)
{
    initialize(capacity);
    std::cout << "[INFO] Server configured on port " << port << " (capacity: " << capacity << ")\n";
}

VeloxKVServer::~VeloxKVServer()
{
    if (running)
    {
        stop();
    }
}

bool VeloxKVServer::initialize(uint32_t cap)
{
    capacity = cap > 0 ? cap : 10000;
    cache = std::make_unique<LRUCache<std::string, std::string>>(capacity);
    cache_initialized = true;
    return true;
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
    if (args.size() >= 2)
    {
        try
        {
            uint32_t cap = std::stoul(args.back());
            if (cap > 0)
            {
                initialize(cap);
            }
        }
        catch (...)
        {
        }
    }
    return "OK: Cache initialized (capacity: " + std::to_string(capacity) + ")";
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
    info += "version: 0.2.0 (Non-blocking Event Loop)\r\n";
    info += "port: " + std::to_string(port) + "\r\n";
    info += "capacity: " + std::to_string(capacity) + "\r\n";
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

void VeloxKVServer::set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void VeloxKVServer::accept_new_client()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
    {
        return;
    }

    set_nonblocking(client_fd);

    pollfd pfd{};
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    poll_fds.push_back(pfd);

    client_sessions[client_fd] = ClientSession{client_fd, ""};
    active_connections.fetch_add(1, std::memory_order_relaxed);
}

void VeloxKVServer::close_client(int client_fd)
{
    ::close(client_fd);
    client_sessions.erase(client_fd);
    for (auto it = poll_fds.begin(); it != poll_fds.end(); ++it)
    {
        if (it->fd == client_fd)
        {
            poll_fds.erase(it);
            break;
        }
    }
    if (active_connections.load() > 0)
    {
        active_connections.fetch_sub(1, std::memory_order_relaxed);
    }
}

void VeloxKVServer::handle_client_data(int client_fd)
{
    char buffer[1024];
    ssize_t bytes_received = ::recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0)
    {
        close_client(client_fd);
        return;
    }

    buffer[bytes_received] = '\0';
    auto &session = client_sessions[client_fd];
    session.read_buffer += std::string(buffer, bytes_received);

    size_t pos = 0;
    while ((pos = session.read_buffer.find('\n')) != std::string::npos)
    {
        std::string line = session.read_buffer.substr(0, pos);
        session.read_buffer.erase(0, pos + 1);

        line.erase(line.find_last_not_of(" \n\r\t") + 1);

        if (line.empty())
        {
            continue;
        }

        std::string response = process_command(line);

        if (line == "QUIT" || line == "EXIT")
        {
            response = "OK\r\n";
            ::send(client_fd, response.c_str(), response.size(), 0);
            close_client(client_fd);
            return;
        }

        response += "\r\n> ";
        ::send(client_fd, response.c_str(), response.size(), 0);
    }
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

    set_nonblocking(server_socket);

    poll_fds.clear();
    client_sessions.clear();

    pollfd server_pfd{};
    server_pfd.fd = server_socket;
    server_pfd.events = POLLIN;
    server_pfd.revents = 0;
    poll_fds.push_back(server_pfd);

    running = true;
    std::cout << "[SERVER] VeloxKV listening on port " << port << " (Non-blocking Event Loop)\n";

    while (running)
    {
        int poll_count = ::poll(poll_fds.data(), poll_fds.size(), 100); // 100ms timeout
        if (poll_count < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        if (poll_count == 0)
        {
            continue;
        }

        if (poll_fds[0].revents & POLLIN)
        {
            accept_new_client();
        }

        for (size_t i = 1; i < poll_fds.size();)
        {
            int client_fd = poll_fds[i].fd;
            short revents = poll_fds[i].revents;

            if (revents & (POLLIN | POLLHUP | POLLERR))
            {
                size_t prev_size = poll_fds.size();
                handle_client_data(client_fd);
                if (poll_fds.size() < prev_size)
                {
                    // Client was removed/closed, do not increment i
                    continue;
                }
            }
            i++;
        }
    }

    stop();
    return true;
}

void VeloxKVServer::stop()
{
    running = false;
    for (auto &pfd : poll_fds)
    {
        if (pfd.fd >= 0)
        {
            ::close(pfd.fd);
        }
    }
    poll_fds.clear();
    client_sessions.clear();
    server_socket = -1;
}
