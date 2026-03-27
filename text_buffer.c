#include "myterm.h"

void text_buffer_init(TextBuffer *buffer) {
    memset(buffer, 0, sizeof(TextBuffer));
    buffer->max_lines = MAX_LINES;
    buffer->scroll_offset = 0;
    buffer->line_count = 0;
}

void text_buffer_add_line(TextBuffer *buffer, const char *line) {
    if (buffer->line_count >= buffer->max_lines) {
        // Shift all lines up by one
        for (int i = 0; i < buffer->max_lines - 1; i++) {
            strcpy(buffer->lines[i], buffer->lines[i + 1]);
        }
        buffer->line_count = buffer->max_lines - 1;
    }
    
    // Add new line
    strncpy(buffer->lines[buffer->line_count], line, MAX_LINE_LENGTH - 1);
    buffer->lines[buffer->line_count][MAX_LINE_LENGTH - 1] = '\0';
    buffer->line_count++;
    
    // Auto-scroll to bottom
    buffer->scroll_offset = 0;
}

void text_buffer_clear(TextBuffer *buffer) {
    buffer->line_count = 0;
    buffer->scroll_offset = 0;
    memset(buffer->lines, 0, sizeof(buffer->lines));
}

void text_buffer_scroll(TextBuffer *buffer, int lines) {
    buffer->scroll_offset += lines;
    
    if (buffer->scroll_offset < 0) {
        buffer->scroll_offset = 0;
    }
    
    int max_scroll = buffer->line_count - 50; // Assume 50 visible lines
    if (max_scroll < 0) max_scroll = 0;
    
    if (buffer->scroll_offset > max_scroll) {
        buffer->scroll_offset = max_scroll;
    }
}

