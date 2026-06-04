#include <iostream>
#include "include/server/tcp_server.hpp"

/*
    VeloxKV Server
    
    Simple TCP server listening on port 6380.
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

int main() {
    std::cout << "\n╔════════════════════════════════════════════╗\n";
    std::cout << "║      VeloxKV Server v0.1.0                 ║\n";
    std::cout << "║  High-Performance In-Memory Cache          ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n\n";
    
    std::cout << "[INFO] Starting server...\n";
    std::cout << "[INFO] Listening on port 6380\n";
    std::cout << "[INFO] Waiting for client to send INIT command\n";
    std::cout << "[INFO] Press Ctrl+C to stop\n\n";
    
    try {
        // Create server (no cache type yet, will be set by client)
        VeloxKVServer server(6380);
        
        // Start listening (blocks)
        server.start();
        
        std::cout << "\n[INFO] Server stopped\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
}
