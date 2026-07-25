#include "../../include/server/tcp_server.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>

/*
    VeloxKV Server

    Simple TCP server listening on port 6380 by default.
    Waits for client to send INIT command to initialize cache.

    Client protocol:
      INIT <type> <capacity>   - Initialize cache (type: 0=serial, 1=concurrent, 2=sharded)
      SET key value            - Set key-value pair
      GET key                  - Get value for key
      DEL key                  - Delete key
      QUIT                     - Disconnect

    Example:
      $ ./velox-kv-server
      Server listening on :6380
      $ telnet localhost 6380
      INIT concurrent 10000
      OK
      SET mykey myvalue
      OK
      GET mykey
      VALUE: myvalue
*/

#include <sstream>
#include <string>

/**
 * @file main.cpp
 * @brief Entry point for the VeloxKV server daemon.
 *
 * Parses command-line arguments, sets up server configurations, and manages
 * the startup and lifecycle of the TCP server.
 */

/**
 * @brief Prints a formatted, thread-safe log message to standard output.
 *
 * @param level Log severity level (INFO, ERROR, etc.).
 * @param msg The message string to print.
 */
void Log(const std::string &level, const std::string &msg)
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    std::cout << "[" << ss.str() << "] [" << level << "] " << msg << std::endl;
}

int main(int argc, char *argv[])
{
    std::cout << "\n╔════════════════════════════════════════════╗\n";
    std::cout << "║      VeloxKV Server v0.1.0                 ║\n";
    std::cout << "║  High-Performance In-Memory Cache          ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n\n";

    uint16_t port = 6380;
    uint32_t capacity = 0;
    std::string type_str = "concurrent";

    /* Parse command-line parameters. */
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc)
        {
            try
            {
                port = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Invalid port argument specified." << std::endl;
                return 1;
            }
        }
        else if ((arg == "-c" || arg == "--capacity") && i + 1 < argc)
        {
            try
            {
                capacity = std::stoul(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "Invalid capacity argument specified." << std::endl;
                return 1;
            }
        }
        else if ((arg == "-t" || arg == "--type") && i + 1 < argc)
        {
            type_str = argv[++i];
        }
        else if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  -p, --port <port>        Port number to listen on (default: 6380)\n"
                      << "  -c, --capacity <cap>     Pre-allocated cache capacity (default: 0, client must INIT)\n"
                      << "  -t, --type <type>        Cache type: serial, concurrent, sharded (default: concurrent)\n"
                      << "  -h, --help               Display configuration guidelines\n";
            return 0;
        }
    }

    Log("INFO", "Initializing VeloxKV Server daemon...");

    try
    {
        VeloxKVServer server(port);

        /* Pre-initialize the cache if capacity is set via CLI. */
        if (capacity > 0)
        {
            VeloxKVServer::CacheType c_type;
            if (type_str == "serial")
            {
                c_type = VeloxKVServer::CacheType::SERIAL;
            }
            else if (type_str == "concurrent")
            {
                c_type = VeloxKVServer::CacheType::CONCURRENT;
            }
            else if (type_str == "sharded")
            {
                c_type = VeloxKVServer::CacheType::SHARDED;
            }
            else
            {
                std::cerr << "[ERROR] Invalid cache type '" << type_str << "'. Must be: serial, concurrent, or sharded."
                          << std::endl;
                return 1;
            }

            Log("INFO", "Pre-configuring " + type_str + " cache (capacity: " + std::to_string(capacity) + ")...");
            if (!server.initialize(c_type, capacity))
            {
                Log("ERROR", "Failed to pre-initialize the cache system.");
                return 1;
            }
        }
        else
        {
            Log("INFO", "Awaiting client INIT command for cache configuration on port " + std::to_string(port));
        }

        Log("INFO", "Starting listener thread on port " + std::to_string(port) + "...");
        server.start();

        Log("INFO", "Server stopped gracefully.");
        return 0;
    }
    catch (const std::exception &e)
    {
        Log("ERROR", std::string("Fatal runtime error: ") + e.what());
        return 1;
    }
}
