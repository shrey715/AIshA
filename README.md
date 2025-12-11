# AIshA

Advanced Intelligent Shell Assistant - A Unix shell with integrated Gemini AI.

## Quick Start

```bash
make
./aisha
```

## AI Features

```bash
# Set up AI (get key from https://aistudio.google.com/apikey)
aikey YOUR_API_KEY

# Translate natural language to commands
ask list all python files modified today

# Explain complex commands
explain find . -name "*.c" -exec grep TODO {} +

# Chat with AI
ai how do I find large files on disk
```

## Commands

**AI:** `ask`, `explain`, `ai`, `aifix`, `aiconfig`, `aikey`

**Navigation:** `cd`, `pwd`, `ls`

**Shell:** `history`, `alias`, `export`, `source`, `exit`

**Jobs:** `jobs`, `fg`, `bg`, `kill`

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Tab | Auto-complete |
| Up/Down | Browse history |
| Ctrl+A/E | Start/end of line |
| Ctrl+K/U | Delete line |
| Ctrl+L | Clear screen |
| Ctrl+C | Cancel |
| Ctrl+D | Exit |

## Configuration

```bash
# ~/.aisharc
GEMINI_API_KEY=your_key

# ~/.aisharc
alias ll="ls -la"
export EDITOR=vim
```

## Architecture

AIshA is built entirely in C99 using POSIX-compliant system libraries. No external dependencies except OpenSSL for HTTPS.

### Core Components

| Module | Purpose |
|--------|---------|
| `src/core/` | Shell initialization, main loop, prompt generation |
| `src/parser/` | Tokenizer and command parser with quote/escape handling |
| `src/builtins/` | 40+ built-in commands split into logical modules |
| `src/editing/` | Custom readline implementation using termios |
| `src/jobs/` | Background process management and signal handling |
| `src/ai/` | Gemini API integration via raw OpenSSL sockets |
| `src/utils/` | ANSI colors, glob patterns, directory utilities |

### POSIX APIs Used

- **Process Control:** `fork()`, `exec()`, `waitpid()`, `setpgid()`
- **Signals:** `sigaction()`, `sigemptyset()`, `kill()`
- **Terminal:** `termios` for raw mode line editing
- **File System:** `opendir()`, `stat()`, `access()`, `getcwd()`
- **Environment:** `getenv()`, `setenv()`, `environ`

### Design Decisions

- **No readline/ncurses dependency** - Custom implementation using termios for portability
- **No libcurl** - Raw OpenSSL sockets for HTTPS to minimize dependencies
- **Modular builtins** - Commands split across 8 files by category
- **JSON parsing** - Bundled cJSON (single header) for Gemini API responses

### Memory Management

All dynamic allocations are tracked and freed on exit. The shell uses:
- Hash tables for variables and aliases
- Circular buffer for command history
- Token arrays for parsing (stack-allocated, fixed size)

## Build

```bash
make          # build
make debug    # debug build with symbols
make release  # optimized build
make install  # install to /usr/local/bin
make loc      # count lines of code by module
make structure # show source tree
```

### Requirements

- GCC or Clang with C99 support
- OpenSSL development libraries (`libssl-dev` / `openssl-devel`)
- POSIX-compliant OS (Linux, macOS, BSD)