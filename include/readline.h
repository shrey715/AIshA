/**
 * @file readline.h
 * @brief Custom line editing and history navigation
 * 
 * Provides a readline-like interface for interactive command input.
 * Implements terminal raw mode handling, cursor movement, history
 * navigation, and kill buffer operations without external dependencies.
 * 
 * Supported features:
 * - Arrow key navigation (left/right for cursor, up/down for history)
 * - Home/End keys
 * - Ctrl+A/E (beginning/end of line)
 * - Ctrl+K/U (kill to end/beginning)
 * - Ctrl+W (kill previous word)
 * - Ctrl+Y (yank/paste)
 * - Ctrl+L (clear screen)
 * - Ctrl+T (transpose characters)
 */

#ifndef READLINE_H
#define READLINE_H

#include <stdlib.h>

/*============================================================================
 * Key Code Definitions
 *============================================================================*/

/** Control key codes */
#define KEY_CTRL_A      1    /**< Ctrl+A - beginning of line */
#define KEY_CTRL_B      2    /**< Ctrl+B - backward char */
#define KEY_CTRL_C      3    /**< Ctrl+C - interrupt */
#define KEY_CTRL_D      4    /**< Ctrl+D - delete/EOF */
#define KEY_CTRL_E      5    /**< Ctrl+E - end of line */
#define KEY_CTRL_F      6    /**< Ctrl+F - forward char */
#define KEY_CTRL_H      8    /**< Ctrl+H - backspace */
#define KEY_TAB         9    /**< Tab - completion */
#define KEY_CTRL_J      10   /**< Ctrl+J - newline */
#define KEY_CTRL_K      11   /**< Ctrl+K - kill to end */
#define KEY_CTRL_L      12   /**< Ctrl+L - clear screen */
#define KEY_ENTER       13   /**< Enter - submit line */
#define KEY_CTRL_N      14   /**< Ctrl+N - next history */
#define KEY_CTRL_P      16   /**< Ctrl+P - previous history */
#define KEY_CTRL_R      18   /**< Ctrl+R - reverse search */
#define KEY_CTRL_T      20   /**< Ctrl+T - transpose */
#define KEY_CTRL_U      21   /**< Ctrl+U - kill to beginning */
#define KEY_CTRL_W      23   /**< Ctrl+W - kill word */
#define KEY_CTRL_Y      25   /**< Ctrl+Y - yank */
#define KEY_ESCAPE      27   /**< Escape - start of sequence */
#define KEY_BACKSPACE   127  /**< Backspace - delete behind */

/** Arrow and special key codes (after escape sequence processing) */
#define KEY_ARROW_UP    1000 /**< Up arrow */
#define KEY_ARROW_DOWN  1001 /**< Down arrow */
#define KEY_ARROW_RIGHT 1002 /**< Right arrow */
#define KEY_ARROW_LEFT  1003 /**< Left arrow */
#define KEY_HOME        1004 /**< Home key */
#define KEY_END         1005 /**< End key */
#define KEY_DELETE      1006 /**< Delete key */
#define KEY_PAGE_UP     1007 /**< Page Up key */
#define KEY_PAGE_DOWN   1008 /**< Page Down key */

/*============================================================================
 * Initialization and Cleanup
 *============================================================================*/

/**
 * Initialize the readline subsystem
 * 
 * Sets up internal buffers and history storage.
 * Call during shell startup.
 */
void readline_init(void);

/**
 * Clean up readline resources
 * 
 * Frees history entries and restores terminal mode.
 * Call during shell shutdown.
 */
void readline_cleanup(void);

/*============================================================================
 * Terminal Mode Control
 *============================================================================*/

/**
 * Enable terminal raw mode
 * 
 * Disables echo, canonical mode, and signal generation.
 * Allows reading individual key presses.
 */
void enable_raw_mode(void);

/**
 * Disable terminal raw mode
 * 
 * Restores original terminal settings.
 */
void disable_raw_mode(void);

/*============================================================================
 * Line Reading
 *============================================================================*/

/**
 * Read a line of input with editing support
 * 
 * Main entry point for interactive input. Displays the prompt,
 * handles all editing keys, and returns the complete line.
 * 
 * For non-interactive mode (no TTY), falls back to simple fgets.
 * 
 * @param prompt Prompt string to display
 * @return Dynamically allocated string with input, or NULL on EOF.
 *         Empty string on Ctrl+C. Caller must free.
 */
char* shell_readline(const char* prompt);

/*============================================================================
 * History Management
 *============================================================================*/

/**
 * Add a line to command history
 * 
 * Skips empty lines and consecutive duplicates.
 * 
 * @param line Command line to add
 */
void history_add(const char* line);

/**
 * Clear all history entries
 */
void history_clear(void);

/**
 * Get a specific history entry
 * 
 * @param index History index (0 = oldest)
 * @return History entry string, or NULL if out of range
 */
const char* history_get(int index);

/**
 * Get current history count
 * 
 * @return Number of entries in history
 */
int history_count(void);

#endif /* READLINE_H */
