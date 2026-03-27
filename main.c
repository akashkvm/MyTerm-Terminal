#include "myterm.h"
#include <locale.h>

// Global application instance
static MyTermApp *g_app = NULL;

void signal_handler(int sig) {
    if (g_app) {
        printf("\nReceived signal %d, shutting down gracefully...\n", sig);
        g_app->running = 0;
    }
}

void setup_signal_handlers(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe signals
}

int myterm_init(MyTermApp *app) {
    memset(app, 0, sizeof(MyTermApp));
    
    // Initialize X11
    if (x11_init(app) != 0) {
        fprintf(stderr, "Failed to initialize X11\n");
        return -1;
    }
    
    // Initialize text buffer
    text_buffer_init(&app->text_buffer);
    
    // Initialize clipboard
    memset(app->clipboard, 0, sizeof(app->clipboard));
    
    // Create first tab
    app->tab_count = 1;
    app->active_tab = 0;
    
    if (tab_create(&app->tabs[0], 0) != 0) {
        fprintf(stderr, "Failed to create initial tab\n");
        return -1;
    }
    
    app->running = 1;
    
    printf("MyTerm initialized successfully\n");
    return 0;
}

void myterm_cleanup(MyTermApp *app) {
    // Stop all shell processes
    for (int i = 0; i < app->tab_count; i++) {
        tab_destroy(&app->tabs[i]);
    }
    
    // Cleanup X11
    x11_cleanup(app);
    
    printf("MyTerm cleanup completed\n");
}

int myterm_run(MyTermApp *app) {
    XEvent event;
    fd_set read_fds;
    int max_fd = 0;
    struct timeval timeout;
    
    printf("Starting main event loop...\n");
    
    while (app->running) {
        // Check for X11 events
        while (XPending(app->display)) {
            XNextEvent(app->display, &event);
            x11_handle_event(app, &event);
        }
        
        // Check for output from all tabs (both shell processes and external commands)
        FD_ZERO(&read_fds);
        max_fd = 0;
        
        for (int i = 0; i < app->tab_count; i++) {
            Tab *tab = &app->tabs[i];
            // Always check pipes for output, regardless of shell_pid
            FD_SET(tab->stdout_pipe[0], &read_fds);
            if (tab->stdout_pipe[0] > max_fd) {
                max_fd = tab->stdout_pipe[0];
            }
            
            FD_SET(tab->stderr_pipe[0], &read_fds);
            if (tab->stderr_pipe[0] > max_fd) {
                max_fd = tab->stderr_pipe[0];
            }
        }
        
        if (max_fd > 0) {
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000; // 10ms
            
            int result = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
            
            if (result > 0) {
                // Handle output from all tabs
                for (int i = 0; i < app->tab_count; i++) {
                    Tab *tab = &app->tabs[i];
                    if (FD_ISSET(tab->stdout_pipe[0], &read_fds)) {
                        shell_process_handle_output(app, tab);
                    }
                    if (FD_ISSET(tab->stderr_pipe[0], &read_fds)) {
                        shell_process_handle_output(app, tab);
                    }
                }
            }
        } else {
            // No active pipes, sleep briefly
            usleep(10000); // 10ms
        }
        
        // Check if any shell processes have died
        for (int i = 0; i < app->tab_count; i++) {
            Tab *tab = &app->tabs[i];
            if (tab->shell_pid > 0) {
                int status;
                pid_t result = waitpid(tab->shell_pid, &status, WNOHANG);
                if (result > 0) {
                    printf("Shell process %d in tab %d has terminated\n", tab->shell_pid, i);
                    tab->shell_pid = 0;
                    
                    // Restart shell if this is the active tab
                    if (i == app->active_tab) {
                        printf("Restarting shell for active tab %d\n", i);
                        shell_process_start(tab);
                    }
                }
            }
        }
    }
    
    printf("Main event loop ended\n");
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    
    // Set locale for Unicode support
    setlocale(LC_ALL, "");
    
    MyTermApp app = {0};
    g_app = &app;
    
    setup_signal_handlers();
    
    printf("Starting MyTerm - Custom Shell with X11 GUI\n");
    
    // Initialize the application
    if (myterm_init(&app) != 0) {
        fprintf(stderr, "Failed to initialize MyTerm\n");
        return 1;
    }
    
    // Run the main event loop
    int result = myterm_run(&app);
    
    // Cleanup
    myterm_cleanup(&app);
    
    printf("MyTerm terminated\n");
    return result;
}
