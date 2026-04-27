CC = cc
CFLAGS = -Wall -Wextra -Werror -std=c11 -pthread -Iinclude
TARGET = greasy_cards
SRC = src/main.c src/game.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC) include/game.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) greasy_cards.log
