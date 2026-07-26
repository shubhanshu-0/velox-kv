# VeloxKV Makefile

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O3
CPPFLAGS ?= -I.
LDLIBS ?= -lpthread

SERVER_SRCS = src/server/main.cpp src/server/tcp_server.cpp
TEST_SRCS = tests/tests.cpp
CLIENT_SRCS = tests/test_client.cpp

SERVER_BIN = velox-kv-server
TEST_BIN = velox-kv-tests
CLIENT_BIN = velox-kv-client

.PHONY: all server test check clean help

# Build everything.
all: $(SERVER_BIN) $(TEST_BIN) $(CLIENT_BIN)

# Build the TCP server binary.
$(SERVER_BIN): $(SERVER_SRCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LDLIBS)
	@echo "✓ Built: $@"

# Build the cache test binary.
$(TEST_BIN): $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LDLIBS)
	@echo "✓ Built: $@"

# Build the interactive client binary.
$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LDLIBS)
	@echo "✓ Built: $@"

# Run the server.
server: $(SERVER_BIN)
	./$(SERVER_BIN)

# Build and run the test suite.
test: $(TEST_BIN)
	./$(TEST_BIN)

check: test

# Clean generated binaries and debug symbols.
clean:
	rm -f $(SERVER_BIN) $(TEST_BIN) $(CLIENT_BIN)
	rm -rf *.dSYM tests/*.dSYM
	@echo "✓ Cleaned"

help:
	@echo "VeloxKV Build Targets:"
	@echo "  make            - Build server, tests, and client"
	@echo "  make server     - Build and run the server"
	@echo "  make test       - Build and run the cache tests"
	@echo "  make check      - Alias for make test"
	@echo "  make clean      - Remove binaries and debug symbols"
	@echo "  make help       - Show this help"
