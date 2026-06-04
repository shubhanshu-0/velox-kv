# VeloxKV Makefile - Simple build with g++

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3
LDFLAGS = -lpthread
INCLUDE = -I.

# Source files
SERVER_SRCS = src/server/main.cpp src/server/tcp_server.cpp
TEST_SRCS = tests/tests.cpp
CLIENT_SRCS = tests/test_client.cpp

# Output binaries
SERVER_BIN = velox-kv-server
TEST_BIN = velox-kv-tests
CLIENT_BIN = velox-kv-client

# Default target
.PHONY: all
all: $(SERVER_BIN) $(TEST_BIN) $(CLIENT_BIN)

# Build server
$(SERVER_BIN): $(SERVER_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built: $@"

# Build tests
$(TEST_BIN): $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built: $@"

# Build test client
$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built: $@"

# Run server
.PHONY: server
server: $(SERVER_BIN)
	./$(SERVER_BIN)

# Run tests
.PHONY: test
test: $(TEST_BIN)
	./$(TEST_BIN)

# Clean
.PHONY: clean
clean:
	rm -f $(SERVER_BIN) $(TEST_BIN) $(CLIENT_BIN)
	@echo "✓ Cleaned"

# Help
.PHONY: help
help:
	@echo "VeloxKV Build Targets:"
	@echo "  make            - Build all (server, tests, client)"
	@echo "  make server     - Build and run server"
	@echo "  make test       - Build and run tests"
	@echo "  make clean      - Remove binaries"
	@echo "  make help       - Show this help"
