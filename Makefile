CC = g++
CFLAGS = -Wall -Wextra -std=c++17
SRC_DIR = src
BUILD_DIR = build

CLIENT_SRC = $(SRC_DIR)/blink_cli.cpp
SERVER_SRC = $(SRC_DIR)/blink_server.cpp
LB_SERVER_SRC = $(SRC_DIR)/blink_server_with_lb.cpp

CLIENT_BIN = $(BUILD_DIR)/blink_cli
SERVER_BIN = $(BUILD_DIR)/blink_server
LB_SERVER_BIN = $(BUILD_DIR)/blink_server_with_lb

all: $(CLIENT_BIN) $(SERVER_BIN) $(LB_SERVER_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(CLIENT_BIN): $(CLIENT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN)

$(LB_SERVER_BIN): $(LB_SERVER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LB_SERVER_SRC) -o $(LB_SERVER_BIN)

.PHONY: clean run_client run_server

run_client: $(CLIENT_BIN)
	$(CLIENT_BIN)

run_server: $(SERVER_BIN)
	$(SERVER_BIN)

run_lb_server: $(LB_SERVER_BIN)
	$(LB_SERVER_BIN)

clean:
	rm -rf $(BUILD_DIR)
