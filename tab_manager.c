#include "myterm.h"

int tab_create(Tab *tab, int id) {
    memset(tab, 0, sizeof(Tab));
    
    tab->id = id;
    tab->active = 0;
    tab->cursor_pos = 0;
    tab->history_count = 0;
    tab->history_index = 0;
    
    // Initialize multiline support
    memset(tab->multiline_buffer, 0, sizeof(tab->multiline_buffer));
    tab->multiline_mode = 0;
    tab->multiline_continuation = 0;
    
    // Initialize text selection support
    tab->selection_start_x = tab->selection_start_y = 0;
    tab->selection_end_x = tab->selection_end_y = 0;
    tab->selection_active = 0;
    memset(tab->selected_text, 0, sizeof(tab->selected_text));
    
    snprintf(tab->title, sizeof(tab->title), "Terminal %d", id + 1);
    
    // Initialize pipes
    tab->stdin_pipe[0] = tab->stdin_pipe[1] = -1;
    tab->stdout_pipe[0] = tab->stdout_pipe[1] = -1;
    tab->stderr_pipe[0] = tab->stderr_pipe[1] = -1;
    
    // Start shell process
    if (shell_process_start(tab) != 0) {
        fprintf(stderr, "Failed to start shell for tab %d\n", id);
        return -1;
    }
    
    printf("Created tab %d with shell PID %d\n", id, tab->shell_pid);
    return 0;
}

void tab_destroy(Tab *tab) {
    if (tab->shell_pid > 0) {
        shell_process_stop(tab);
    }
    
    memset(tab, 0, sizeof(Tab));
}

void tab_switch(MyTermApp *app, int tab_id) {
    if (tab_id < 0 || tab_id >= app->tab_count) {
        return;
    }
    
    // Deactivate current tab
    if (app->active_tab >= 0 && app->active_tab < app->tab_count) {
        app->tabs[app->active_tab].active = 0;
    }
    
    // Activate new tab
    app->active_tab = tab_id;
    app->tabs[app->active_tab].active = 1;
    
    // Clear text buffer for new tab (each tab should have its own buffer)
    text_buffer_clear(&app->text_buffer);
    
    printf("Switched to tab %d\n", tab_id);
    x11_redraw(app);
}

void tab_add_new(MyTermApp *app) {
    if (app->tab_count >= MAX_TABS) {
        printf("Maximum number of tabs reached\n");
        return;
    }
    
    int new_tab_id = app->tab_count;
    
    if (tab_create(&app->tabs[new_tab_id], new_tab_id) == 0) {
        app->tab_count++;
        
        // Switch to the new tab
        tab_switch(app, new_tab_id);
        
        printf("Added new tab %d\n", new_tab_id);
    }
}
