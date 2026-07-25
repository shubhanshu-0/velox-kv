#include "../../include/server/tcp_server.hpp"
#include "../../include/core/cache_manager.hpp"
#include "../../include/policies/lru.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

// POSIX socket headers
#include <arpa/inet.h>  // inet_pton()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <unistd.h>     // close(), read(), write()

/*
    VeloxKV TCP Server Implementation

    Socket Flow:
    1. socket()   → Create a socket
    2. bind()     → Bind to port 6380
    3. listen()   → Start listening for connections
    4. accept()   → Wait for client (BLOCKS until someone connects)
    5. recv()     → Read data from client
    6. send()     → Send response to client
    7. close()    → Close connection
*/

// Constructor: Initialize server
VeloxKVServer::VeloxKVServer(uint16_t p) : server_socket(-1), port(p), running(false), cache_initialized(false)
{

    std::cout << "[INFO] Server ready on port " << port << " (cache pending INIT command)\n";
}

// Destructor: cleanup
VeloxKVServer::~VeloxKVServer()
{
    if (running)
    {
        stop();
    }
}

// Parse command string into tokens
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

// Handle INIT command: INIT <type> <capacity>
// Types: 0=SERIAL, 1=CONCURRENT, 2=SHARDED (or text: serial, concurrent, sharded)
std::string VeloxKVServer::handle_init(const std::vector<std::string> &args)
{
    if (cache_initialized)
    {
        return "ERROR: Cache already initialized. Cannot reinitialize.";
    }

    if (args.size() < 3)
    {
        return "ERROR: INIT requires 'INIT <type> <capacity>' (type: 0/serial, 1/concurrent, 2/sharded)";
    }

    // Parse cache type - accept both numeric (0,1,2) and text (serial, concurrent, sharded)
    int type_val = -1;
    std::string type_str = args[1];

    // Convert to lowercase for text comparison
    std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::tolower);

    // Try numeric first
    try
    {
        type_val = std::stoi(type_str);
    }
    catch (...)
    {
        // Try text format
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
        return "ERROR: Invalid cache type. Must be 0/serial, 1/concurrent, or 2/sharded";
    }

    if (type_val < 0 || type_val > 2)
    {
        return "ERROR: Invalid cache type. Must be 0 (serial), 1 (concurrent), or 2 (sharded)";
    }

    // Parse capacity
    try
    {
        capacity = std::stoul(args[2]);
    }
    catch (...)
    {
        return "ERROR: Invalid capacity. Must be a positive integer";
    }

    if (capacity == 0)
    {
        return "ERROR: Capacity must be > 0";
    }

    cache_type = static_cast<CacheType>(type_val);

    // Factory function to create LRU cache
    auto factory = [](uint32_t c) { return std::make_unique<LRUCache<std::string, std::string>>(c); };

    // Create cache instance based on type
    try
    {
        switch (cache_type)
        {
        case CacheType::SERIAL:
            std::cout << "[INIT] Creating SERIAL cache (capacity: " << capacity << ")\n";
            cache = std::make_unique<SerialCache<std::string, std::string>>(factory, capacity);
            break;

        case CacheType::CONCURRENT:
            std::cout << "[INIT] Creating CONCURRENT cache (capacity: " << capacity << ")\n";
            cache = std::make_unique<ConcurrentCache<std::string, std::string>>(factory, capacity);
            break;

        case CacheType::SHARDED:
            std::cout << "[INIT] Creating SHARDED cache (capacity: " << capacity << ")\n";
            cache = std::make_unique<ShardedConcurrentCache<std::string, std::string>>(factory, capacity);
            break;
        }

        cache_initialized = true;
        std::cout << "[INIT] Cache initialized successfully\n";
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
        return "OK: " + type_name + " cache ready (capacity: " + std::to_string(capacity) + ")";
    }
    catch (const std::exception &e)
    {
        std::cout << "[INIT] Failed to create cache: " << e.what() << "\n";
        return "ERROR: Failed to initialize cache: " + std::string(e.what());
    }
}

// Handle GET command: GET key

std::string VeloxKVServer::handle_get(const std::string &key)
{
    std::cout << "[GET] Looking for key='" << key << "'\n";
    auto value = cache->get(key);
    if (value)
    {
        std::cout << "[GET] Found: '" << *value << "'\n";
        return "VALUE " + *value;
    }
    std::cout << "[GET] Not found\n";
    return "nil";
}

// Handle SET command: SET key value [expiration]
std::string VeloxKVServer::handle_set(const std::string &args)
{
    auto tokens = parse_command(args);
    if (tokens.size() < 3)
    {
        return "ERROR: SET requires 'SET key value [expiration]'";
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
            return "ERROR: Invalid expiration time";
        }
    }

    std::cout << "[SET] key='" << key << "' value='" << value << "' exp=" << expiration << "\n";
    cache->set(key, value, expiration);
    std::cout << "[SET] Done\n";
    return "OK";
}

// Handle DEL command: DEL key
std::string VeloxKVServer::handle_del(const std::string &key)
{
    bool deleted = cache->remove(key);
    return deleted ? "1" : "0";
}

// Handle FLUSH command: remove all keys
std::string VeloxKVServer::handle_flush()
{
    // Simple flush: just return OK
    // (In production, would iterate and remove all)
    return "OK";
}

// Handle STATS command
std::string VeloxKVServer::handle_stats()
{
    return "STATS: Server running normally";
}

// Handle INFO command
std::string VeloxKVServer::handle_info()
{
    std::string info = "# VeloxKV Server\r\n";
    info += "version: 0.1.0\r\n";
    info += "port: " + std::to_string(port) + "\r\n";

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
    return info;
}

// Handle QUIT command
std::string VeloxKVServer::handle_quit()
{
    return "OK";
}

// Handle HELP command
std::string VeloxKVServer::handle_help()
{
    return "Commands: GET, SET, DEL, FLUSH, STATS, INFO, HELP, QUIT";
}

// Process a command and return response
std::string VeloxKVServer::process_command(const std::string &line)
{
    auto tokens = parse_command(line);

    if (tokens.empty())
    {
        return "ERROR: Empty command";
    }

    // Convert command to uppercase
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    // INIT must be the first command, before cache is initialized
    if (cmd == "INIT")
    {
        return handle_init(tokens);
    }

    // All other commands require cache to be initialized
    if (!cache_initialized)
    {
        return "ERROR: Cache not initialized. Send INIT command first.";
    }

    // Dispatch to appropriate handler
    if (cmd == "GET")
    {
        if (tokens.size() < 2)
            return "ERROR: GET requires key";
        return handle_get(tokens[1]);
    }
    else if (cmd == "SET")
    {
        return handle_set(line); // Pass full line to parse properly
    }
    else if (cmd == "DEL")
    {
        if (tokens.size() < 2)
            return "ERROR: DEL requires key";
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
        return "ERROR: Unknown command '" + cmd + "'. Type HELP for commands.";
    }
}

// Handle a single client connection (runs in separate thread)
void VeloxKVServer::handle_client(int client_socket)
{
    char buffer[1024];

    std::cout << "[CLIENT] New client thread started\n";

    // Read commands from client in a loop
    while (running)
    {
        memset(buffer, 0, sizeof(buffer));

        // RECV: Read data from client socket
        // - client_socket: the connected client
        // - buffer: where to store received data
        // - sizeof(buffer)-1: max bytes to read (leave space for null terminator)
        // - 0: flags (0 = blocking mode)
        // Returns: number of bytes received, or -1 on error, or 0 if disconnected
        ssize_t bytes_received = ::recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received < 0)
        {
            std::cerr << "[CLIENT] Recv error\n";
            break;
        }

        if (bytes_received == 0)
        {
            std::cout << "[CLIENT] Client disconnected (recv returned 0)\n";
            break;
        }

        // Null-terminate the received data
        buffer[bytes_received] = '\0';
        std::string command(buffer);

        // Remove trailing whitespace/newlines
        command.erase(command.find_last_not_of(" \n\r\t") + 1);

        if (command.empty())
        {
            continue;
        }

        std::cout << "[CLIENT] Received: " << command << "\n";

        // Process the command
        std::string response = process_command(command);

        // Check for QUIT/EXIT
        if (command == "QUIT" || command == "EXIT")
        {
            response = "OK";
            ::send(client_socket, (response + "\r\n").c_str(), response.size() + 2, 0);
            std::cout << "[CLIENT] Client requested quit\n";
            break;
        }

        // SEND: Send response back to client
        // - client_socket: the connected client
        // - response.c_str(): data to send
        // - response.size(): number of bytes to send
        // - 0: flags (0 = normal sending)
        // Returns: number of bytes sent, or -1 on error
        response += "\r\n> "; // Add prompt
        ssize_t bytes_sent = ::send(client_socket, response.c_str(), response.size(), 0);

        if (bytes_sent < 0)
        {
            std::cerr << "[CLIENT] Send error\n";
            break;
        }

        std::cout << "[CLIENT] Sent response\n";
    }

    // CLOSE: Close the client socket
    ::close(client_socket);
    std::cout << "[CLIENT] Client thread ended\n";
}

// Start server (blocks, listening for clients)
bool VeloxKVServer::start()
{
    // SOCKET: Create a socket
    // - AF_INET: IPv4 protocol family
    // - SOCK_STREAM: TCP (stream) socket
    // - 0: protocol (0 = default for SOCK_STREAM, which is TCP)
    // Returns: socket file descriptor (>= 0), or -1 on error
    server_socket = ::socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        std::cerr << "[ERROR] Socket creation failed\n";
        return false;
    }

    std::cout << "[SERVER] Socket created (fd=" << server_socket << ")\n";

    // Allow reusing the port (avoid "Address already in use" error)
    int opt = 1;
    if (::setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[ERROR] Setsockopt failed\n";
        return false;
    }

    // BIND: Bind socket to a specific port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces (0.0.0.0)
    server_addr.sin_port = htons(port);       // Convert port to network byte order

    if (::bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[ERROR] Bind to port " << port << " failed\n";
        ::close(server_socket);
        return false;
    }

    std::cout << "[SERVER] Bound to port " << port << "\n";

    // LISTEN: Start listening for incoming connections
    // - server_socket: the listening socket
    // - 10: backlog (queue up to 10 pending connections)
    // Returns: 0 on success, -1 on error
    if (::listen(server_socket, 10) < 0)
    {
        std::cerr << "[ERROR] Listen failed\n";
        ::close(server_socket);
        return false;
    }

    running = true;
    std::cout << "[SERVER] ✓ Listening on port " << port << "\n";
    std::cout << "[SERVER] Waiting for clients...\n";

    // ACCEPT loop: Wait for client connections (BLOCKS HERE)
    while (running)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // ACCEPT: Accept an incoming client connection
        // - server_socket: the listening socket
        // - (sockaddr*)&client_addr: where to store client's address
        // - &client_len: size of address structure
        // Returns: new socket file descriptor for this client, or -1 on error
        // BLOCKS until a client connects
        int client_socket = ::accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket < 0)
        {
            if (running)
            {
                std::cerr << "[ERROR] Accept failed\n";
            }
            break;
        }

        std::cout << "[SERVER] ✓ Client connected (new socket fd=" << client_socket << ")\n";

        // Create new thread to handle this client
        // Lambda captures: [this, client_socket]
        // - this: access to member functions
        // - client_socket: the client's socket
        std::thread client_thread([this, client_socket]() { this->handle_client(client_socket); });

        // Detach thread (let it run independently)
        // Server continues accepting more clients
        client_thread.detach();
    }

    // Clean up
    ::close(server_socket);
    std::cout << "[SERVER] Server stopped\n";

    return true;
}

// Stop server
void VeloxKVServer::stop()
{
    running = false;
    if (server_socket >= 0)
    {
        ::close(server_socket);
        server_socket = -1;
    }
    std::cout << "[SERVER] ✓ Server stopped\n";
}
