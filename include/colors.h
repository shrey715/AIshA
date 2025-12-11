#ifndef COLORS_H
#define COLORS_H

#include <stdio.h>
#include <unistd.h>

/* Check if output supports colors */
#define COLORS_SUPPORTED (isatty(STDOUT_FILENO))

/* Reset */
#define COLOR_RESET     "\033[0m"

/* Regular Colors */
#define COLOR_BLACK     "\033[0;30m"
#define COLOR_RED       "\033[0;31m"
#define COLOR_GREEN     "\033[0;32m"
#define COLOR_YELLOW    "\033[0;33m"
#define COLOR_BLUE      "\033[0;34m"
#define COLOR_MAGENTA   "\033[0;35m"
#define COLOR_CYAN      "\033[0;36m"
#define COLOR_WHITE     "\033[0;37m"

/* Bold Colors */
#define COLOR_BOLD_BLACK    "\033[1;30m"
#define COLOR_BOLD_RED      "\033[1;31m"
#define COLOR_BOLD_GREEN    "\033[1;32m"
#define COLOR_BOLD_YELLOW   "\033[1;33m"
#define COLOR_BOLD_BLUE     "\033[1;34m"
#define COLOR_BOLD_MAGENTA  "\033[1;35m"
#define COLOR_BOLD_CYAN     "\033[1;36m"
#define COLOR_BOLD_WHITE    "\033[1;37m"

/* Background Colors */
#define COLOR_BG_BLACK      "\033[40m"
#define COLOR_BG_RED        "\033[41m"
#define COLOR_BG_GREEN      "\033[42m"
#define COLOR_BG_YELLOW     "\033[43m"
#define COLOR_BG_BLUE       "\033[44m"
#define COLOR_BG_MAGENTA    "\033[45m"
#define COLOR_BG_CYAN       "\033[46m"
#define COLOR_BG_WHITE      "\033[47m"

/* Text Attributes */
#define COLOR_BOLD          "\033[1m"
#define COLOR_DIM           "\033[2m"
#define COLOR_ITALIC        "\033[3m"
#define COLOR_UNDERLINE     "\033[4m"
#define COLOR_BLINK         "\033[5m"
#define COLOR_REVERSE       "\033[7m"
#define COLOR_HIDDEN        "\033[8m"
#define COLOR_STRIKETHROUGH "\033[9m"

/* File type colors (for ls-like output) */
#define COLOR_DIR           COLOR_BOLD_BLUE
#define COLOR_LINK          COLOR_BOLD_CYAN
#define COLOR_EXEC          COLOR_BOLD_GREEN
#define COLOR_ARCHIVE       COLOR_BOLD_RED
#define COLOR_IMAGE         COLOR_BOLD_MAGENTA
#define COLOR_AUDIO         COLOR_CYAN
#define COLOR_VIDEO         COLOR_BOLD_MAGENTA
#define COLOR_DOC           COLOR_WHITE
#define COLOR_SOCKET        COLOR_BOLD_MAGENTA
#define COLOR_PIPE          COLOR_YELLOW
#define COLOR_BLOCK         COLOR_BOLD_YELLOW
#define COLOR_CHAR          COLOR_BOLD_YELLOW
#define COLOR_ORPHAN        COLOR_BOLD_RED
#define COLOR_SETUID        "\033[37;41m"
#define COLOR_SETGID        "\033[30;43m"
#define COLOR_STICKY        "\033[30;44m"
#define COLOR_OTHER_WRITABLE "\033[34;42m"

/* Error and status colors */
#define COLOR_ERROR         COLOR_BOLD_RED
#define COLOR_WARNING       COLOR_BOLD_YELLOW
#define COLOR_SUCCESS       COLOR_BOLD_GREEN
#define COLOR_INFO          COLOR_BOLD_CYAN

/* Prompt colors */
#define COLOR_PROMPT_USER   COLOR_BOLD_GREEN
#define COLOR_PROMPT_HOST   COLOR_BOLD_GREEN
#define COLOR_PROMPT_PATH   COLOR_BOLD_BLUE
#define COLOR_PROMPT_SYMBOL COLOR_BOLD_WHITE

/* Helper macros for conditional coloring */
#define PRINT_COLOR(color) (COLORS_SUPPORTED ? color : "")

/* Utility functions */
void print_colored(const char* color, const char* text);
void print_error(const char* format, ...);
void print_warning(const char* format, ...);
void print_success(const char* format, ...);
void print_info(const char* format, ...);

/* Get color for file type based on mode */
const char* get_file_color(unsigned int mode, const char* filename);

/* Get color for file extension */
const char* get_extension_color(const char* filename);

#endif /* COLORS_H */
