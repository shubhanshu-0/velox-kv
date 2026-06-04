#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
    VeloxKV Test Client
    Simple command-line client to test the server
    
    Usage:
      $ ./velox-kv-client
      Connected to localhost:6380
      > INIT concurrent 10000
      OK
      > SET key value
      OK
      > GET key
      VALUE: value
      > QUIT
*/


int main() {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }
    
    // Connect to server
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6380);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Connection failed to localhost:6380\n";
        std::cerr << "Make sure server is running: ./velox-kv-server\n";
        close(sock);
        return 1;
    }
    
    std::cout << "✓ Connected to localhost:6380\n";
    std::cout << "Type commands (INIT, SET, GET, DEL, QUIT, HELP)\n\n";
    
    std::string command;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);
        
        if (command.empty()) continue;
        
        // Send command
        send(sock, command.c_str(), command.size(), 0);
        send(sock, "\n", 1, 0);
        
        // Receive response
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes <= 0) {
            std::cout << "✗ Connection closed\n";
            break;
        }
        
        buffer[bytes] = '\0';
        std::cout << buffer;
        
        if (command == "QUIT") {
            break;
        }
    }
    
    close(sock);
    std::cout << "Disconnected\n";
    return 0;
}
