#ifndef MYTERM_H
#define MYTERM_H

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

// Constants
#define MAX_TABS 5
#define MAX_LINES 100
#define MAX_LINE_LENGTH 256
#define TAB_HEIGHT 30
#define FONT_SIZE 14
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Colors
#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_GRAY 0x808080
#define COLOR_DARK_GRAY 0x404040
#define COLOR_BLUE 0x0000FF
#define COLOR_GREEN 0x00FF00
#define COLOR_RED 0xFF0000

// Tab structure
typedef struct {
    int id;
    char title[64];
    int active;
    pid_t shell_pid;
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    char current_line[MAX_LINE_LENGTH];
    int cursor_pos;
    char history[MAX_LINES][MAX_LINE_LENGTH];
    int history_count;
    int history_index;
    
    // Multiline input support
    char multiline_buffer[MAX_LINE_LENGTH * 4];  // Buffer for multiline input
    int multiline_mode;  // 1 if in multiline mode, 0 otherwise
    int multiline_continuation;  // 1 if last line ended with backslash
    
    // Text selection support
    int selection_start_x, selection_start_y;  // Selection start coordinates
    int selection_end_x, selection_end_y;      // Selection end coordinates
    int selection_active;  // 1 if text is currently selected
    char selected_text[MAX_LINE_LENGTH * 4];   // Currently selected text
} Tab;

// Text buffer structure
typedef struct {
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int line_count;
    int scroll_offset;
    int max_lines;
} TextBuffer;

// Main application structure
typedef struct {
    Display *display;
    Window window;
    GC gc;
    XFontStruct *font;
    XFontSet fontset;  // For Unicode support
    int screen;
    int depth;
    Visual *visual;
    Colormap colormap;
    
    Tab tabs[MAX_TABS];
    int tab_count;
    int active_tab;
    
    TextBuffer text_buffer;
    
    // Clipboard support
    char clipboard[MAX_LINE_LENGTH * 4];  // Global clipboard buffer
    
    int running;
    int width;
    int height;
} MyTermApp;

// Function declarations
// Main application
int myterm_init(MyTermApp *app);
void myterm_cleanup(MyTermApp *app);
int myterm_run(MyTermApp *app);

// X11 GUI functions
int x11_init(MyTermApp *app);
void x11_cleanup(MyTermApp *app);
void x11_handle_event(MyTermApp *app, XEvent *event);
void x11_redraw(MyTermApp *app);
void x11_draw_text(MyTermApp *app, int x, int y, const char *text, unsigned long color);
int x11_text_width(MyTermApp *app, const char *text, int len);
void x11_draw_tabs(MyTermApp *app);
void x11_draw_terminal(MyTermApp *app);

// Text buffer functions
void text_buffer_init(TextBuffer *buffer);
void text_buffer_add_line(TextBuffer *buffer, const char *line);
void text_buffer_clear(TextBuffer *buffer);
void text_buffer_scroll(TextBuffer *buffer, int lines);

// Tab management
int tab_create(Tab *tab, int id);
void tab_destroy(Tab *tab);
void tab_switch(MyTermApp *app, int tab_id);
void tab_add_new(MyTermApp *app);

// Shell process functions
int shell_process_start(Tab *tab);
void shell_process_stop(Tab *tab);
int shell_process_write(Tab *tab, const char *data, int len);
int shell_process_read(Tab *tab, char *buffer, int max_len);
void shell_process_handle_output(MyTermApp *app, Tab *tab);

// External command execution
int execute_external_command(MyTermApp *app, Tab *tab, const char *command);
int execute_command_with_input_redirection(MyTermApp *app, Tab *tab, const char *command);
int execute_builtin_command(MyTermApp *app, Tab *tab, const char *command);
int parse_command(const char *command, char **argv, int max_args);
int parse_command_with_redirection(const char *command, char **argv, int max_args, char **input_file, char **output_file);
int is_builtin_command(const char *command);
int execute_cd_command(Tab *tab, const char *path);

// Pipe command execution
int parse_pipe_command(const char *command, char **commands, int max_commands);
int execute_pipe_command(MyTermApp *app, Tab *tab, const char *command);

// MultiWatch command execution
int parse_multiwatch_commands(const char *command, char **commands, int max_commands);
int execute_multiwatch_command(MyTermApp *app, Tab *tab, const char *command);
void multiwatch_signal_handler(int sig);

// History management
void save_command_to_history(const char *cmd);
void trim_history_file(void);
int execute_history_command(MyTermApp *app, Tab *tab, const char *command);
char* find_exact_match(const char *term);
void find_and_print_best_substring_matches(const char *term, MyTermApp *app);

// Input handling
void handle_keypress(MyTermApp *app, XKeyEvent *event);
void handle_button_press(MyTermApp *app, XButtonEvent *event);
void handle_motion_notify(MyTermApp *app, XMotionEvent *event);
void handle_button_release(MyTermApp *app, XButtonEvent *event);
void handle_resize(MyTermApp *app, XConfigureEvent *event);
void process_single_command(MyTermApp *app, Tab *active_tab);
void process_multiline_command(MyTermApp *app, Tab *active_tab);
void handle_history_search(MyTermApp *app, Tab *tab);

// Copy-paste functionality
void copy_to_clipboard(MyTermApp *app, const char *text);
void paste_from_clipboard(MyTermApp *app, Tab *tab);
void clear_selection(Tab *tab);
void update_selection(MyTermApp *app, Tab *tab, int x, int y);

// Utility functions
void signal_handler(int sig);
void setup_signal_handlers(void);

#endif // MYTERM_H
