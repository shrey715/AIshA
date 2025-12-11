# AIshA Makefile
# Advanced Intelligent Shell Assistant

CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
         -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm

# OpenSSL for HTTPS/AI features
LDFLAGS = -lssl -lcrypto

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Source subdirectories
SUBDIRS = core parser utils editing jobs builtins ai

# Collect all source files from all subdirectories
SOURCES := $(wildcard $(SRCDIR)/*.c)
SOURCES += $(foreach dir,$(SUBDIRS),$(wildcard $(SRCDIR)/$(dir)/*.c))

# Generate object file names - flatten all into obj/ with unique names
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.c))
OBJECTS += $(patsubst $(SRCDIR)/core/%.c,$(OBJDIR)/core_%.o,$(wildcard $(SRCDIR)/core/*.c))
OBJECTS += $(patsubst $(SRCDIR)/parser/%.c,$(OBJDIR)/parser_%.o,$(wildcard $(SRCDIR)/parser/*.c))
OBJECTS += $(patsubst $(SRCDIR)/utils/%.c,$(OBJDIR)/utils_%.o,$(wildcard $(SRCDIR)/utils/*.c))
OBJECTS += $(patsubst $(SRCDIR)/editing/%.c,$(OBJDIR)/editing_%.o,$(wildcard $(SRCDIR)/editing/*.c))
OBJECTS += $(patsubst $(SRCDIR)/jobs/%.c,$(OBJDIR)/jobs_%.o,$(wildcard $(SRCDIR)/jobs/*.c))
OBJECTS += $(patsubst $(SRCDIR)/builtins/%.c,$(OBJDIR)/builtins_%.o,$(wildcard $(SRCDIR)/builtins/*.c))
OBJECTS += $(patsubst $(SRCDIR)/ai/%.c,$(OBJDIR)/ai_%.o,$(wildcard $(SRCDIR)/ai/*.c))

# Target binary
TARGET = aisha

# Version info
VERSION = 2.0.0

# Default target
all: $(TARGET)

# Create object directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile rules for each subdirectory
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/core_%.o: $(SRCDIR)/core/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/parser_%.o: $(SRCDIR)/parser/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/utils_%.o: $(SRCDIR)/utils/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/editing_%.o: $(SRCDIR)/editing/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/jobs_%.o: $(SRCDIR)/jobs/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/builtins_%.o: $(SRCDIR)/builtins/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/ai_%.o: $(SRCDIR)/ai/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Link all object files
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TARGET)

# Release build with optimizations
release: CFLAGS += -O2 -DNDEBUG
release: clean $(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET) shell.out

# Install to /usr/local/bin
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/aisha

# Uninstall
uninstall:
	rm -f /usr/local/bin/aisha

# Run the shell
run: $(TARGET)
	./$(TARGET)

# Run with valgrind for memory checking
memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Format source files (requires clang-format)
format:
	find $(SRCDIR) $(INCDIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

# Count lines of code
loc:
	@echo "Lines of code by module:"
	@for dir in $(SUBDIRS); do \
		echo -n "  $$dir: "; \
		find $(SRCDIR)/$$dir -name "*.c" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $$1}'; \
	done
	@echo "  root: $$(wc -l $(SRCDIR)/*.c 2>/dev/null | tail -1 | awk '{print $$1}')"
	@echo "Total:"
	@find $(SRCDIR) $(INCDIR) -name "*.c" -o -name "*.h" | xargs wc -l | tail -1

# Show source structure
structure:
	@echo "AIshA Source Structure:"
	@echo ""
	@echo "src/"
	@for dir in $(SUBDIRS); do \
		echo "├── $$dir/"; \
		ls $(SRCDIR)/$$dir/*.c 2>/dev/null | sed 's|$(SRCDIR)/$$dir/|│   ├── |'; \
	done
	@echo "└── (root files)"
	@ls $(SRCDIR)/*.c 2>/dev/null | sed 's|$(SRCDIR)/|    ├── |'

# Show help
help:
	@echo "AIshA Makefile - Advanced Intelligent Shell Assistant"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build the shell (default)"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build with optimizations"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to /usr/local/bin"
	@echo "  run       - Build and run the shell"
	@echo "  memcheck  - Run with valgrind"
	@echo "  format    - Format source code"
	@echo "  loc       - Count lines of code by module"
	@echo "  structure - Show source tree"
	@echo "  help      - Show this help"

.PHONY: all clean debug release install uninstall run memcheck format loc structure help
