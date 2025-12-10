CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm

# Directories
SRCDIR = src
INCDIR = include

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)

TARGET = shell.out

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

clean:
	rm -f $(TARGET) $(SRCDIR)/*.o

.PHONY: all clean debug
