CC = g++
CFLAGS = -Wall -Wextra -std=c++17
SRC_DIR = src
BUILD_DIR = build

CLIENT_SRC = $(SRC_DIR)/blink_cli.cpp
SERVER_SRC = $(SRC_DIR)/blink_server.cpp
CLIENT_BIN = $(BUILD_DIR)/blink_cli
SERVER_BIN = $(BUILD_DIR)/blink_server

all: $(CLIENT_BIN) $(SERVER_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(CLIENT_BIN): $(CLIENT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN)

.PHONY: clean run_client run_server

run_client: $(CLIENT_BIN)
	$(CLIENT_BIN)

run_server: $(SERVER_BIN)
	$(SERVER_BIN)

clean:
	rm -rf $(BUILD_DIR)
