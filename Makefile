# Makefile for tg-ws-proxy-linux
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lpthread -lssl -lcrypto

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(filter-out $(SRC_DIR)/mtproto_test.c, $(wildcard $(SRC_DIR)/*.c))
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = tg-ws-proxy

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

tests: all $(SRC_DIR)/mtproto_test.c
	$(CC) $(CFLAGS) -o mtproto_test $(SRC_DIR)/mtproto_test.c $(LDFLAGS)
	@echo "Starting proxy in background for testing..."
	./$(TARGET) 8080 & \
	PROXY_PID=$$!; \
	sleep 1; \
	./mtproto_test; \
	TEST_RES=$$?; \
	kill $$PROXY_PID; \
	exit $$TEST_RES

install: all
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "Installed $(TARGET) to /usr/local/bin/$(TARGET)"

clean:
	rm -rf $(OBJ_DIR) $(TARGET) mtproto_test

.PHONY: all tests install clean
