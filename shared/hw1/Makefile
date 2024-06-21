# Makefile for compiling the user-space test program

# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -Wall -std=c99

# Target executable
TARGET = main

# Source files
SRCS = main.c

# Default target
all: $(TARGET)

# Rule to build the target
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean up build files
clean:
	rm -f $(TARGET) *.o
