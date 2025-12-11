#include "readline.h"
#include "colors.h"
#include "completion.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>

/* Terminal state */
static struct termios orig_termios;
static int raw_mode_enabled = 0;

/* Line buffer */
#define LINE_BUFFER_SIZE 4096
static char line_buffer[LINE_BUFFER_SIZE];
static int line_length = 0;
static int cursor_pos = 0;

/* History */
#define HISTORY_SIZE 1000
static char* history[HISTORY_SIZE];
static int history_len = 0;
static int history_index = 0;

/* Kill buffer (for Ctrl+K, Ctrl+Y) */
static char kill_buffer[LINE_BUFFER_SIZE];
static int kill_len = 0;

/* Search state - for future Ctrl+R implementation */
/* static char search_buffer[256]; */
/* static int search_len = 0; */
/* static int searching = 0; */

void readline_init(void) {
    memset(history, 0, sizeof(history));
    history_len = 0;
    memset(kill_buffer, 0, sizeof(kill_buffer));
}

void readline_cleanup(void) {
    history_clear();
    if (raw_mode_enabled) {
        disable_raw_mode();
    }
}

void enable_raw_mode(void) {
    if (raw_mode_enabled) return;
    if (!isatty(STDIN_FILENO)) return;
    
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    
    struct termios raw = orig_termios;
    
    /* Input flags: no break, no CR to NL, no parity check, no strip, no flow control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    
    /* Output flags: disable post processing */
    raw.c_oflag &= ~(OPOST);
    
    /* Control flags: set 8 bit chars */
    raw.c_cflag |= (CS8);
    
    /* Local flags: no echo, no canonical, no extended, no signal chars */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    /* Control chars: read returns after 1 byte, no timeout */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
    
    raw_mode_enabled = 1;
}

void disable_raw_mode(void) {
    if (!raw_mode_enabled) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_mode_enabled = 0;
}

/* Get terminal width - for future multiline support */
/*
static int get_terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return 80;
    }
    return ws.ws_col;
}
*/

/* Read a single key, handling escape sequences */
static int read_key(void) {
    char c;
    int nread;
    
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1) return -1;
    }
    
    if (c == KEY_ESCAPE) {
        char seq[3];
        
        /* Try to read escape sequence */
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESCAPE;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESCAPE;
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return KEY_ESCAPE;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DELETE;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_ARROW_UP;
                    case 'B': return KEY_ARROW_DOWN;
                    case 'C': return KEY_ARROW_RIGHT;
                    case 'D': return KEY_ARROW_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
            }
        }
        
        return KEY_ESCAPE;
    }
    
    return (unsigned char)c;
}

/* Refresh the line display */
static void refresh_line(const char* prompt, int prompt_len) {
    char buf[LINE_BUFFER_SIZE * 2];
    int pos = 0;
    
    /* Move cursor to column 0 */
    buf[pos++] = '\r';
    
    /* Write prompt and buffer */
    int written = snprintf(buf + pos, sizeof(buf) - pos, "%s%.*s", 
                           prompt, line_length, line_buffer);
    pos += written;
    
    /* Clear to end of line */
    buf[pos++] = '\033';
    buf[pos++] = '[';
    buf[pos++] = 'K';
    
    /* Move cursor to correct position */
    int cursor_col = prompt_len + cursor_pos;
    snprintf(buf + pos, sizeof(buf) - pos, "\r\033[%dC", cursor_col);
    
    write(STDOUT_FILENO, buf, strlen(buf));
}

/* Insert character at cursor position */
static void insert_char(char c) {
    if (line_length >= LINE_BUFFER_SIZE - 1) return;
    
    if (cursor_pos < line_length) {
        memmove(line_buffer + cursor_pos + 1, 
                line_buffer + cursor_pos, 
                line_length - cursor_pos);
    }
    line_buffer[cursor_pos] = c;
    line_length++;
    cursor_pos++;
    line_buffer[line_length] = '\0';
}

/* Delete character at cursor position */
static void delete_char(void) {
    if (cursor_pos < line_length) {
        memmove(line_buffer + cursor_pos, 
                line_buffer + cursor_pos + 1, 
                line_length - cursor_pos);
        line_length--;
        line_buffer[line_length] = '\0';
    }
}

/* Delete character before cursor (backspace) */
static void backspace_char(void) {
    if (cursor_pos > 0) {
        cursor_pos--;
        delete_char();
    }
}

/* Set line from history */
static void set_line_from_history(int index) {
    if (index < 0 || index >= history_len) return;
    
    const char* hist = history[history_len - 1 - index];
    if (hist) {
        strncpy(line_buffer, hist, LINE_BUFFER_SIZE - 1);
        line_buffer[LINE_BUFFER_SIZE - 1] = '\0';
        line_length = strlen(line_buffer);
        cursor_pos = line_length;
    }
}

/* History search - for future Ctrl+R implementation */
/*
static int search_history(const char* pattern, int start_index, int direction) {
    for (int i = start_index; i >= 0 && i < history_len; i += direction) {
        if (strstr(history[i], pattern)) {
            return i;
        }
    }
    return -1;
}
*/

char* shell_readline(const char* prompt) {
    /* Calculate prompt length without ANSI codes for cursor positioning */
    int prompt_len = 0;
    int in_escape = 0;
    for (const char* p = prompt; *p; p++) {
        if (*p == '\033') {
            in_escape = 1;
        } else if (in_escape) {
            if (*p == 'm') in_escape = 0;
        } else {
            prompt_len++;
        }
    }
    
    /* Initialize line state */
    line_buffer[0] = '\0';
    line_length = 0;
    cursor_pos = 0;
    history_index = history_len;
    
    /* Check if we're in a terminal */
    if (!isatty(STDIN_FILENO)) {
        /* Non-interactive mode - just read a line */
        if (fgets(line_buffer, LINE_BUFFER_SIZE, stdin) == NULL) {
            return NULL;
        }
        /* Remove trailing newline */
        int len = strlen(line_buffer);
        if (len > 0 && line_buffer[len - 1] == '\n') {
            line_buffer[len - 1] = '\0';
        }
        return strdup(line_buffer);
    }
    
    /* Print prompt */
    write(STDOUT_FILENO, prompt, strlen(prompt));
    
    enable_raw_mode();
    
    while (1) {
        int key = read_key();
        
        if (key == -1) {
            disable_raw_mode();
            return NULL;
        }
        
        switch (key) {
            case KEY_ENTER:
            case KEY_CTRL_J:
                disable_raw_mode();
                write(STDOUT_FILENO, "\n", 1);
                line_buffer[line_length] = '\0';
                return strdup(line_buffer);
                
            case KEY_CTRL_D:
                if (line_length == 0) {
                    /* EOF on empty line */
                    disable_raw_mode();
                    return NULL;
                }
                delete_char();
                refresh_line(prompt, prompt_len);
                break;
                
            case KEY_CTRL_C:
                disable_raw_mode();
                write(STDOUT_FILENO, "^C\n", 3);
                line_buffer[0] = '\0';
                return strdup("");
                
            case KEY_BACKSPACE:
            case KEY_CTRL_H:
                backspace_char();
                refresh_line(prompt, prompt_len);
                break;
                
            case KEY_DELETE:
                delete_char();
                refresh_line(prompt, prompt_len);
                break;
                
            case KEY_ARROW_LEFT:
            case KEY_CTRL_B:
                if (cursor_pos > 0) {
                    cursor_pos--;
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_ARROW_RIGHT:
            case KEY_CTRL_F:
                if (cursor_pos < line_length) {
                    cursor_pos++;
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_ARROW_UP:
            case KEY_CTRL_P:
                if (history_index > 0) {
                    history_index--;
                    set_line_from_history(history_len - 1 - history_index);
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_ARROW_DOWN:
            case KEY_CTRL_N:
                if (history_index < history_len) {
                    history_index++;
                    if (history_index == history_len) {
                        line_buffer[0] = '\0';
                        line_length = 0;
                        cursor_pos = 0;
                    } else {
                        set_line_from_history(history_len - 1 - history_index);
                    }
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_HOME:
            case KEY_CTRL_A:
                cursor_pos = 0;
                refresh_line(prompt, prompt_len);
                break;
                
            case KEY_END:
            case KEY_CTRL_E:
                cursor_pos = line_length;
                refresh_line(prompt, prompt_len);
                break;
                
            case KEY_CTRL_K:
                /* Kill to end of line */
                if (cursor_pos < line_length) {
                    strncpy(kill_buffer, line_buffer + cursor_pos, LINE_BUFFER_SIZE);
                    kill_len = line_length - cursor_pos;
                    line_length = cursor_pos;
                    line_buffer[line_length] = '\0';
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_CTRL_U:
                /* Kill to beginning of line */
                if (cursor_pos > 0) {
                    strncpy(kill_buffer, line_buffer, cursor_pos);
                    kill_len = cursor_pos;
                    memmove(line_buffer, line_buffer + cursor_pos, line_length - cursor_pos + 1);
                    line_length -= cursor_pos;
                    cursor_pos = 0;
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_CTRL_W:
                /* Kill previous word */
                if (cursor_pos > 0) {
                    int old_pos = cursor_pos;
                    while (cursor_pos > 0 && line_buffer[cursor_pos - 1] == ' ') cursor_pos--;
                    while (cursor_pos > 0 && line_buffer[cursor_pos - 1] != ' ') cursor_pos--;
                    
                    int deleted = old_pos - cursor_pos;
                    strncpy(kill_buffer, line_buffer + cursor_pos, deleted);
                    kill_len = deleted;
                    
                    memmove(line_buffer + cursor_pos, line_buffer + old_pos, line_length - old_pos + 1);
                    line_length -= deleted;
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_CTRL_Y:
                /* Yank (paste) kill buffer */
                if (kill_len > 0) {
                    for (int i = 0; i < kill_len; i++) {
                        insert_char(kill_buffer[i]);
                    }
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_CTRL_L:
                /* Clear screen */
                write(STDOUT_FILENO, "\033[2J\033[H", 7);
                write(STDOUT_FILENO, prompt, strlen(prompt));
                refresh_line(prompt, prompt_len);
                break;
                
            case KEY_CTRL_T:
                /* Transpose characters */
                if (cursor_pos > 0 && cursor_pos < line_length) {
                    char tmp = line_buffer[cursor_pos - 1];
                    line_buffer[cursor_pos - 1] = line_buffer[cursor_pos];
                    line_buffer[cursor_pos] = tmp;
                    if (cursor_pos < line_length) cursor_pos++;
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_TAB:
                /* Tab completion */
                {
                    completion_result_t* result = get_completions(line_buffer, cursor_pos);
                    if (result && result->count > 0) {
                        /* Find word start */
                        int word_start = cursor_pos;
                        while (word_start > 0 && line_buffer[word_start - 1] != ' ' && 
                               line_buffer[word_start - 1] != '\t') {
                            word_start--;
                        }
                        int word_len = cursor_pos - word_start;
                        
                        if (result->count == 1) {
                            /* Single completion - insert it */
                            const char* completion = result->completions[0];
                            int comp_len = strlen(completion);
                            
                            /* Replace word with completion */
                            int new_length = line_length - word_len + comp_len;
                            if (new_length < LINE_BUFFER_SIZE) {
                                memmove(line_buffer + word_start + comp_len, 
                                        line_buffer + cursor_pos, 
                                        line_length - cursor_pos + 1);
                                memcpy(line_buffer + word_start, completion, comp_len);
                                line_length = new_length;
                                cursor_pos = word_start + comp_len;
                                
                                /* Add space if not a directory */
                                if (completion[comp_len - 1] != '/') {
                                    if (line_length < LINE_BUFFER_SIZE - 1) {
                                        memmove(line_buffer + cursor_pos + 1, 
                                                line_buffer + cursor_pos, 
                                                line_length - cursor_pos + 1);
                                        line_buffer[cursor_pos] = ' ';
                                        line_length++;
                                        cursor_pos++;
                                    }
                                }
                            }
                        } else {
                            /* Multiple completions */
                            const char* prefix = result->common_prefix;
                            int prefix_len = strlen(prefix);
                            
                            if (prefix_len > word_len) {
                                /* Complete to common prefix */
                                int new_length = line_length - word_len + prefix_len;
                                if (new_length < LINE_BUFFER_SIZE) {
                                    memmove(line_buffer + word_start + prefix_len, 
                                            line_buffer + cursor_pos, 
                                            line_length - cursor_pos + 1);
                                    memcpy(line_buffer + word_start, prefix, prefix_len);
                                    line_length = new_length;
                                    cursor_pos = word_start + prefix_len;
                                }
                            } else {
                                /* Show all completions */
                                write(STDOUT_FILENO, "\n", 1);
                                
                                int max_len = 0;
                                for (int i = 0; i < result->count; i++) {
                                    int len = strlen(result->completions[i]);
                                    if (len > max_len) max_len = len;
                                }
                                
                                int term_width = 80;
                                int cols = term_width / (max_len + 2);
                                if (cols < 1) cols = 1;
                                
                                for (int i = 0; i < result->count; i++) {
                                    printf("%-*s  ", max_len, result->completions[i]);
                                    if ((i + 1) % cols == 0 || i == result->count - 1) {
                                        printf("\n");
                                    }
                                }
                                
                                /* Redraw prompt and line */
                                write(STDOUT_FILENO, prompt, strlen(prompt));
                            }
                        }
                        completion_free(result);
                    } else {
                        /* No completions - beep */
                        write(STDOUT_FILENO, "\a", 1);
                        if (result) completion_free(result);
                    }
                    refresh_line(prompt, prompt_len);
                }
                break;
                
            case KEY_CTRL_R:
                /* Reverse search - simplified version */
                /* TODO: Implement full interactive search */
                break;
                
            default:
                if (key >= 32 && key < 127) {
                    insert_char((char)key);
                    refresh_line(prompt, prompt_len);
                }
                break;
        }
    }
}

void history_add(const char* line) {
    if (!line || !*line) return;
    
    /* Don't add duplicates of the last entry */
    if (history_len > 0 && strcmp(history[history_len - 1], line) == 0) {
        return;
    }
    
    /* Remove oldest if full */
    if (history_len >= HISTORY_SIZE) {
        free(history[0]);
        memmove(history, history + 1, (HISTORY_SIZE - 1) * sizeof(char*));
        history_len--;
    }
    
    history[history_len++] = strdup(line);
}

void history_clear(void) {
    for (int i = 0; i < history_len; i++) {
        free(history[i]);
        history[i] = NULL;
    }
    history_len = 0;
}

const char* history_get(int index) {
    if (index < 0 || index >= history_len) return NULL;
    return history[index];
}

int history_count(void) {
    return history_len;
}
