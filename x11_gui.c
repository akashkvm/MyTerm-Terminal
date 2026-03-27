#include "myterm.h"

int x11_init(MyTermApp *app) {
    // Open display
    app->display = XOpenDisplay(NULL);
    if (!app->display) {
        fprintf(stderr, "Cannot open display\n");
        return -1;
    }
    
    app->screen = DefaultScreen(app->display);
    app->depth = DefaultDepth(app->display, app->screen);
    app->visual = DefaultVisual(app->display, app->screen);
    app->colormap = DefaultColormap(app->display, app->screen);
    
    // Create window
    app->window = XCreateSimpleWindow(
        app->display,
        RootWindow(app->display, app->screen),
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT,
        1,
        BlackPixel(app->display, app->screen),
        WhitePixel(app->display, app->screen)
    );
    
    // Set window properties
    XStoreName(app->display, app->window, "MyTerm - Custom Shell");
    
    // Select events
    XSelectInput(app->display, app->window,
        ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
        PointerMotionMask | StructureNotifyMask | FocusChangeMask | EnterWindowMask | LeaveWindowMask);
    
    // Create graphics context
    app->gc = XCreateGC(app->display, app->window, 0, NULL);
    XSetForeground(app->display, app->gc, BlackPixel(app->display, app->screen));
    
    // Load font
    app->font = XLoadQueryFont(app->display, "fixed");
    if (!app->font) {
        // Try alternative fonts
        app->font = XLoadQueryFont(app->display, "6x13");
        if (!app->font) {
            app->font = XLoadQueryFont(app->display, "9x15");
            if (!app->font) {
                fprintf(stderr, "Failed to load any font\n");
                return -1;
            }
        }
    }
    
    XSetFont(app->display, app->gc, app->font->fid);
    
    // Create Unicode font set for multilingual support
    char **missing_list;
    int missing_count;
    char *def_string;
    
    // Try to create a Unicode font set
    app->fontset = XCreateFontSet(app->display, 
        "-misc-fixed-*-*-*-*-18-*-*-*-*-*-*-*", 
        &missing_list, &missing_count, &def_string);
    
    if (!app->fontset) {
        // Try alternative font names
        app->fontset = XCreateFontSet(app->display, 
            "fixed", 
            &missing_list, &missing_count, &def_string);
    }
    
    if (!app->fontset) {
        // Fallback to basic font if Unicode font set fails
        fprintf(stderr, "Warning: Failed to create Unicode font set, using basic font\n");
        app->fontset = NULL;
    }
    
    if (missing_list) {
        XFreeStringList(missing_list);
    }
    
    // Map window
    XMapWindow(app->display, app->window);
    
    // Make window take focus when clicked
    XWMHints *wm_hints = XAllocWMHints();
    wm_hints->input = True;
    wm_hints->flags = InputHint;
    XSetWMHints(app->display, app->window, wm_hints);
    XFree(wm_hints);
    
    XFlush(app->display);
    
    app->width = WINDOW_WIDTH;
    app->height = WINDOW_HEIGHT;
    
    return 0;
}

void x11_cleanup(MyTermApp *app) {
    if (app->font) {
        XFreeFont(app->display, app->font);
    }
    if (app->gc) {
        XFreeGC(app->display, app->gc);
    }
    if (app->window) {
        XDestroyWindow(app->display, app->window);
    }
    if (app->display) {
        XCloseDisplay(app->display);
    }
}

void x11_handle_event(MyTermApp *app, XEvent *event) {
    switch (event->type) {
        case Expose:
            if (event->xexpose.count == 0) {
                x11_redraw(app);
            }
            break;
            
        case KeyPress:
            handle_keypress(app, &event->xkey);
            break;
            
        case KeyRelease:
            break;
            
        case ButtonPress:
            handle_button_press(app, &event->xbutton);
            break;
            
        case ButtonRelease:
            handle_button_release(app, &event->xbutton);
            break;
            
        case MotionNotify:
            handle_motion_notify(app, &event->xmotion);
            break;
            
        case ConfigureNotify:
            handle_resize(app, &event->xconfigure);
            break;
            
        case FocusIn:
            break;
            
        case FocusOut:
            break;
            
        case EnterNotify:
            break;
            
        case LeaveNotify:
            break;
    }
}

void x11_redraw(MyTermApp *app) {
    // Clear window
    XClearWindow(app->display, app->window);
    
    // Draw tabs
    x11_draw_tabs(app);
    
    // Draw terminal content
    x11_draw_terminal(app);
    
    XFlush(app->display);
}

void x11_draw_text(MyTermApp *app, int x, int y, const char *text, unsigned long color) {
    XSetForeground(app->display, app->gc, color);
    
    // Use Unicode font set if available, otherwise fall back to basic font
    if (app->fontset) {
        Xutf8DrawString(app->display, app->window, app->fontset, app->gc, x, y, text, strlen(text));
    } else {
        XDrawString(app->display, app->window, app->gc, x, y, text, strlen(text));
    }
}

// Calculate text width using the same method as drawing
int x11_text_width(MyTermApp *app, const char *text, int len) {
    if (app->fontset) {
        // For Unicode fontset, we need to use Xutf8TextExtents
        XRectangle logical_rect;
        Xutf8TextExtents(app->fontset, text, len, NULL, &logical_rect);
        return logical_rect.width;
    } else {
        // For basic font, use XTextWidth
        return XTextWidth(app->font, text, len);
    }
}

void process_single_command(MyTermApp *app, Tab *active_tab) {
    // Add to history
    if (active_tab->history_count < MAX_LINES) {
        strcpy(active_tab->history[active_tab->history_count], active_tab->current_line);
        active_tab->history_count++;
    }
    active_tab->history_index = active_tab->history_count;
    
    // Save to persistent history file
    save_command_to_history(active_tab->current_line);
    
    // Display the command in the terminal
    char display_cmd[MAX_LINE_LENGTH + 20];
    snprintf(display_cmd, sizeof(display_cmd), "user@myterm> %s", active_tab->current_line);
    text_buffer_add_line(&app->text_buffer, display_cmd);
    
    // Check if it's a built-in command or external command
    if (is_builtin_command(active_tab->current_line)) {
        execute_builtin_command(app, active_tab, active_tab->current_line);
    } else {
        execute_external_command(app, active_tab, active_tab->current_line);
    }
}

void process_multiline_command(MyTermApp *app, Tab *active_tab) {
    // Add to history
    if (active_tab->history_count < MAX_LINES) {
        strcpy(active_tab->history[active_tab->history_count], active_tab->multiline_buffer);
        active_tab->history_count++;
    }
    active_tab->history_index = active_tab->history_count;
    
    // Display the multiline command in the terminal
    char display_cmd[MAX_LINE_LENGTH * 4 + 20];
    snprintf(display_cmd, sizeof(display_cmd), "user@myterm> %s", active_tab->multiline_buffer);
    text_buffer_add_line(&app->text_buffer, display_cmd);
    
    // Process the multiline command
    if (is_builtin_command(active_tab->multiline_buffer)) {
        execute_builtin_command(app, active_tab, active_tab->multiline_buffer);
    } else {
        execute_external_command(app, active_tab, active_tab->multiline_buffer);
    }
}

// Copy-paste functionality
void copy_to_clipboard(MyTermApp *app, const char *text) {
    strncpy(app->clipboard, text, sizeof(app->clipboard) - 1);
    app->clipboard[sizeof(app->clipboard) - 1] = '\0';
}

void paste_from_clipboard(MyTermApp *app, Tab *tab) {
    if (strlen(app->clipboard) == 0) return;
    
    int clipboard_len = strlen(app->clipboard);
    int current_len = strlen(tab->current_line);
    
    // Check if there's enough space
    if (current_len + clipboard_len >= MAX_LINE_LENGTH - 1) {
        return; // Not enough space
    }
    
    // Insert clipboard content at cursor position
    memmove(tab->current_line + tab->cursor_pos + clipboard_len,
            tab->current_line + tab->cursor_pos,
            current_len - tab->cursor_pos + 1);
    
    memcpy(tab->current_line + tab->cursor_pos, app->clipboard, clipboard_len);
    tab->cursor_pos += clipboard_len;
}

void clear_selection(Tab *tab) {
    tab->selection_active = 0;
    memset(tab->selected_text, 0, sizeof(tab->selected_text));
}

// Handle history search (Ctrl+R)
void handle_history_search(MyTermApp *app, Tab *tab) {
    // Display search prompt
    char search_prompt[MAX_LINE_LENGTH + 50];
    snprintf(search_prompt, sizeof(search_prompt), "Enter search term:");
    text_buffer_add_line(&app->text_buffer, search_prompt);
    x11_redraw(app);
    
    // For now, we'll implement a simple search that shows the last few commands
    // In a full implementation, this would open a search dialog
    // For this demo, we'll show a message about the feature
    char search_msg[MAX_LINE_LENGTH + 50];
    snprintf(search_msg, sizeof(search_msg), "History search feature activated. Use 'history' command to view command history.");
    text_buffer_add_line(&app->text_buffer, search_msg);
    x11_redraw(app);
}

void update_selection(MyTermApp *app, Tab *tab, int x, int y) {
    if (tab->selection_active) {
        // Update selection end position
        tab->selection_end_x = x;
        tab->selection_end_y = y;
        
        // Calculate character positions based on actual font metrics
        int prompt_width = x11_text_width(app, "user@myterm> ", 13);
        int start_x = 10 + prompt_width;
        
        // Calculate start and end character positions
        int start_pos = 0;
        int end_pos = 0;
        
        // Find start position
        if (tab->selection_start_x >= start_x) {
            int start_offset = tab->selection_start_x - start_x;
            for (int i = 0; i <= (int)strlen(tab->current_line); i++) {
                int width = x11_text_width(app, tab->current_line, i);
                if (width >= start_offset) {
                    start_pos = i;
                    break;
                }
            }
        }
        
        // Find end position
        if (x >= start_x) {
            int end_offset = x - start_x;
            for (int i = 0; i <= (int)strlen(tab->current_line); i++) {
                int width = x11_text_width(app, tab->current_line, i);
                if (width >= end_offset) {
                    end_pos = i;
                    break;
                }
            }
        }
        
        // Ensure start_pos <= end_pos
        if (start_pos > end_pos) {
            int temp = start_pos;
            start_pos = end_pos;
            end_pos = temp;
        }
        
        // Extract selected text
        if (start_pos >= 0 && end_pos <= strlen(tab->current_line) && start_pos < end_pos) {
            int len = end_pos - start_pos;
            strncpy(tab->selected_text, tab->current_line + start_pos, len);
            tab->selected_text[len] = '\0';
        } else {
            tab->selected_text[0] = '\0';
        }
    }
}

void x11_draw_tabs(MyTermApp *app) {
    int tab_width = app->width / app->tab_count;
    if (tab_width < 100) tab_width = 100;
    
    for (int i = 0; i < app->tab_count; i++) {
        int x = i * tab_width;
        int y = 0;
        
        // Draw tab background
        if (i == app->active_tab) {
            XSetForeground(app->display, app->gc, COLOR_WHITE);
        } else {
            XSetForeground(app->display, app->gc, COLOR_GRAY);
        }
        
        XFillRectangle(app->display, app->window, app->gc, x, y, tab_width, TAB_HEIGHT);
        
        // Draw tab border
        XSetForeground(app->display, app->gc, COLOR_BLACK);
        XDrawRectangle(app->display, app->window, app->gc, x, y, tab_width, TAB_HEIGHT);
        
        // Draw tab title
        unsigned long text_color = (i == app->active_tab) ? COLOR_BLACK : COLOR_DARK_GRAY;
        x11_draw_text(app, x + 10, y + TAB_HEIGHT/2 + FONT_SIZE/2, app->tabs[i].title, text_color);
    }
}

void x11_draw_terminal(MyTermApp *app) {
    Tab *active_tab = &app->tabs[app->active_tab];
    int start_y = TAB_HEIGHT + 10;
    int line_height = app->font->ascent + app->font->descent + 2;
    int max_visible_lines = (app->height - start_y - 20) / line_height;
    
    // Draw terminal background
    XSetForeground(app->display, app->gc, COLOR_BLACK);
    XFillRectangle(app->display, app->window, app->gc, 0, TAB_HEIGHT, app->width, app->height - TAB_HEIGHT);
    
    // Calculate scroll offset to show the most recent lines
    int total_lines = app->text_buffer.line_count;
    int scroll_offset = 0;
    if (total_lines > max_visible_lines - 1) { // -1 to leave space for input line
        scroll_offset = total_lines - (max_visible_lines - 1);
    }
    
    // Draw text lines from the buffer
    int lines_drawn = 0;
    for (int i = scroll_offset; i < total_lines && lines_drawn < max_visible_lines - 1; i++) {
        int y = start_y + lines_drawn * line_height + app->font->ascent;
        x11_draw_text(app, 10, y, app->text_buffer.lines[i], COLOR_GREEN);
        lines_drawn++;
    }
    
    // Draw current input line after the last output line
    int input_y = start_y + lines_drawn * line_height + app->font->ascent;
    char input_display[MAX_LINE_LENGTH + 20];
    snprintf(input_display, sizeof(input_display), "user@myterm> %s", active_tab->current_line);
    
    // Draw the prompt first
    x11_draw_text(app, 10, input_y, "user@myterm> ", COLOR_WHITE);
    
    // Draw the input text with selection highlighting
    int prompt_width = x11_text_width(app, "user@myterm> ", 13);
    int text_x = 10 + prompt_width;
    
    if (active_tab->selection_active && strlen(active_tab->selected_text) > 0) {
        // Draw selected text with different color
        x11_draw_text(app, text_x, input_y, active_tab->current_line, COLOR_BLUE);
    } else {
        // Draw normal text
        x11_draw_text(app, text_x, input_y, active_tab->current_line, COLOR_WHITE);
    }
    
    // Draw cursor aligned to text baseline using proper font metrics
    // Calculate exact cursor position based on text width
    int text_width = x11_text_width(app, active_tab->current_line, active_tab->cursor_pos);
    int cursor_x = text_x + text_width;
    
    XSetForeground(app->display, app->gc, COLOR_WHITE);
    
    // Get font metrics for proper cursor alignment
    int ascent = app->font->ascent;
    int descent = app->font->descent;
    int font_height = ascent + descent;
    
    // Draw cursor aligned to text baseline
    int cursor_top = input_y - ascent;
    int cursor_bottom = cursor_top + font_height;
    XDrawLine(app->display, app->window, app->gc, cursor_x, cursor_top, cursor_x, cursor_bottom);
}

void handle_keypress(MyTermApp *app, XKeyEvent *event) {
    Tab *active_tab = &app->tabs[app->active_tab];
    KeySym keysym = XLookupKeysym(event, 0);
    
    // Handle Ctrl+B (copy), Ctrl+V (paste), Ctrl+C (interrupt), Ctrl+A (start of line), Ctrl+E (end of line), Ctrl+R (history search)
    if (event->state & ControlMask) {
        if (keysym == XK_b || keysym == XK_B) {
            // Ctrl+B - Copy selected text
            if (active_tab->selection_active && strlen(active_tab->selected_text) > 0) {
                copy_to_clipboard(app, active_tab->selected_text);
                clear_selection(active_tab);
                x11_redraw(app);
            }
            return;
        } else if (keysym == XK_v || keysym == XK_V) {
            // Ctrl+V - Paste from clipboard
            paste_from_clipboard(app, active_tab);
            clear_selection(active_tab);
            x11_redraw(app);
            return;
        } else if (keysym == XK_c || keysym == XK_C) {
            // Ctrl+C - Send SIGINT to running process
            if (active_tab->shell_pid > 0) {
                kill(active_tab->shell_pid, SIGINT);
            }
            return;
        } else if (keysym == XK_a || keysym == XK_A) {
            // Ctrl+A - Move cursor to start of line
            active_tab->cursor_pos = 0;
            x11_redraw(app);
            return;
        } else if (keysym == XK_e || keysym == XK_E) {
            // Ctrl+E - Move cursor to end of line
            active_tab->cursor_pos = strlen(active_tab->current_line);
            x11_redraw(app);
            return;
        } else if (keysym == XK_r || keysym == XK_R) {
            // Ctrl+R - History search
            handle_history_search(app, active_tab);
            return;
        } else if (keysym == XK_z || keysym == XK_Z) {
            // Ctrl+Z - Send SIGTSTP to running process (background)
            if (active_tab->shell_pid > 0) {
                kill(active_tab->shell_pid, SIGTSTP);
                char bg_msg[MAX_LINE_LENGTH + 50];
                snprintf(bg_msg, sizeof(bg_msg), "[%d] Stopped and moved to background.", active_tab->shell_pid);
                text_buffer_add_line(&app->text_buffer, bg_msg);
                active_tab->shell_pid = 0; // Clear the PID since it's now in background
                x11_redraw(app);
            }
            return;
        }
    }
    
    // Clear selection on any other keypress
    if (active_tab->selection_active) {
        clear_selection(active_tab);
        x11_redraw(app);
    }
    
    // Handle special keys
    if (keysym == XK_Return || keysym == XK_KP_Enter) {
        // Handle multiline input
        if (strlen(active_tab->current_line) > 0) {
            // Check if line ends with backslash (multiline continuation)
            int line_len = strlen(active_tab->current_line);
            if (line_len > 0 && active_tab->current_line[line_len - 1] == '\\') {
                // Remove the backslash and add to multiline buffer
                active_tab->current_line[line_len - 1] = '\0';
                strcat(active_tab->multiline_buffer, active_tab->current_line);
                strcat(active_tab->multiline_buffer, "\n");
                
                // Display the continuation line
                char display_cmd[MAX_LINE_LENGTH + 20];
                snprintf(display_cmd, sizeof(display_cmd), "user@myterm> %s\\", active_tab->current_line);
                text_buffer_add_line(&app->text_buffer, display_cmd);
                
                // Clear current line and continue
                memset(active_tab->current_line, 0, sizeof(active_tab->current_line));
                active_tab->cursor_pos = 0;
                active_tab->multiline_mode = 1;
                active_tab->multiline_continuation = 1;
            } else {
                // End of multiline input or single line
                if (active_tab->multiline_mode) {
                    // Complete multiline input
                    strcat(active_tab->multiline_buffer, active_tab->current_line);
                    
                    // Process the complete multiline command
                    process_multiline_command(app, active_tab);
                    
                    // Reset multiline state
                    memset(active_tab->multiline_buffer, 0, sizeof(active_tab->multiline_buffer));
                    active_tab->multiline_mode = 0;
                    active_tab->multiline_continuation = 0;
                } else {
                    // Single line command
                    process_single_command(app, active_tab);
                }
                
                // Clear current line
                memset(active_tab->current_line, 0, sizeof(active_tab->current_line));
                active_tab->cursor_pos = 0;
            }
        }
    } else if (keysym == XK_BackSpace) {
        // Backspace
        if (active_tab->cursor_pos > 0) {
            active_tab->cursor_pos--;
            active_tab->current_line[active_tab->cursor_pos] = '\0';
        }
    } else if (keysym == XK_Up) {
        // History up
        if (active_tab->history_index > 0) {
            active_tab->history_index--;
            strcpy(active_tab->current_line, active_tab->history[active_tab->history_index]);
            active_tab->cursor_pos = strlen(active_tab->current_line);
        }
    } else if (keysym == XK_Down) {
        // History down
        if (active_tab->history_index < active_tab->history_count - 1) {
            active_tab->history_index++;
            strcpy(active_tab->current_line, active_tab->history[active_tab->history_index]);
            active_tab->cursor_pos = strlen(active_tab->current_line);
        } else {
            // Clear current line
            memset(active_tab->current_line, 0, sizeof(active_tab->current_line));
            active_tab->cursor_pos = 0;
            active_tab->history_index = active_tab->history_count;
        }
    } else {
        // Handle printable characters
        char buffer[32];
        int len = XLookupString(event, buffer, sizeof(buffer) - 1, NULL, NULL);
        buffer[len] = '\0';
        
        if (len == 1) {
            char c = buffer[0];
            if (c >= 32 && c <= 126) {
                // Printable character
                if (active_tab->cursor_pos < MAX_LINE_LENGTH - 1) {
                    active_tab->current_line[active_tab->cursor_pos] = c;
                    active_tab->cursor_pos++;
                }
            }
        }
    }
    
    x11_redraw(app);
}

void handle_button_press(MyTermApp *app, XButtonEvent *event) {
    Tab *active_tab = &app->tabs[app->active_tab];
    
    if (event->y < TAB_HEIGHT) {
        // Clicked on tab area
        int tab_width = app->width / app->tab_count;
        if (tab_width < 100) tab_width = 100;
        
        int clicked_tab = event->x / tab_width;
        if (clicked_tab >= 0 && clicked_tab < app->tab_count) {
            tab_switch(app, clicked_tab);
        }
    } else if (event->button == Button1) {
        // Left mouse button - start text selection
        // Only start selection if clicking in the input area
        int start_y = TAB_HEIGHT + 10;
        int line_height = app->font->ascent + app->font->descent + 2;
        int max_visible_lines = (app->height - start_y - 20) / line_height;
        int total_lines = app->text_buffer.line_count;
        int scroll_offset = 0;
        if (total_lines > max_visible_lines - 1) {
            scroll_offset = total_lines - (max_visible_lines - 1);
        }
        int lines_drawn = 0;
        for (int i = scroll_offset; i < total_lines && lines_drawn < max_visible_lines - 1; i++) {
            lines_drawn++;
        }
        int input_y = start_y + lines_drawn * line_height;
        
        if (event->y >= input_y && event->y <= input_y + line_height) {
            active_tab->selection_start_x = event->x;
            active_tab->selection_start_y = event->y;
            active_tab->selection_end_x = event->x;
            active_tab->selection_end_y = event->y;
            active_tab->selection_active = 1;
            clear_selection(active_tab);
        }
    }
}

void handle_button_release(MyTermApp *app, XButtonEvent *event) {
    Tab *active_tab = &app->tabs[app->active_tab];
    
    if (event->button == Button1 && active_tab->selection_active) {
        // End text selection
        active_tab->selection_end_x = event->x;
        active_tab->selection_end_y = event->y;
        
        // Update selection text
        update_selection(app, active_tab, event->x, event->y);
        
        // Redraw to show selection
        x11_redraw(app);
    }
}

void handle_motion_notify(MyTermApp *app, XMotionEvent *event) {
    Tab *active_tab = &app->tabs[app->active_tab];
    
    if (active_tab->selection_active) {
        // Update selection during mouse drag
        active_tab->selection_end_x = event->x;
        active_tab->selection_end_y = event->y;
        
        // Update selection text
        update_selection(app, active_tab, event->x, event->y);
        
        // Redraw to show selection
        x11_redraw(app);
    }
}

void handle_resize(MyTermApp *app, XConfigureEvent *event) {
    app->width = event->width;
    app->height = event->height;
    x11_redraw(app);
}

