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

tests: $(SRC_DIR)/mtproto_test.c
	$(CC) $(CFLAGS) -o mtproto_test $(SRC_DIR)/mtproto_test.c $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) mtproto_test

.PHONY: all tests clean
