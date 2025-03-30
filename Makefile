CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LDFLAGS = $(shell sdl2-config --libs) -lm

TARGET = cupid-8
SRC = src/cupid-8.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(SRC) -o $(TARGET) $(SDL_LDFLAGS)

clean:
	rm -f $(TARGET)