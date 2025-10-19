# Makefile

CC = gcc
CFLAGS = -Wall -g -std=c99
SOURCES = emulator.c kernel.c interruptionController.c genericProcess.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = os_simulator

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
