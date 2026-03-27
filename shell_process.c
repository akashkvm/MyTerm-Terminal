#include "myterm.h"

int shell_process_start(Tab *tab) {
    // Create pipes for stdin, stdout, stderr
    if (pipe(tab->stdin_pipe) == -1) {
        perror("pipe stdin");
        return -1;
    }
    
    if (pipe(tab->stdout_pipe) == -1) {
        perror("pipe stdout");
        close(tab->stdin_pipe[0]);
        close(tab->stdin_pipe[1]);
        return -1;
    }
    
    if (pipe(tab->stderr_pipe) == -1) {
        perror("pipe stderr");
        close(tab->stdin_pipe[0]);
        close(tab->stdin_pipe[1]);
        close(tab->stdout_pipe[0]);
        close(tab->stdout_pipe[1]);
        return -1;
    }
    
    // Fork process
    tab->shell_pid = fork();
    
    if (tab->shell_pid == -1) {
        perror("fork");
        close(tab->stdin_pipe[0]);
        close(tab->stdin_pipe[1]);
        close(tab->stdout_pipe[0]);
        close(tab->stdout_pipe[1]);
        close(tab->stderr_pipe[0]);
        close(tab->stderr_pipe[1]);
        return -1;
    }
    
    if (tab->shell_pid == 0) {
        // Child process
        // Close unused pipe ends
        close(tab->stdin_pipe[1]);  // Close write end of stdin pipe
        close(tab->stdout_pipe[0]); // Close read end of stdout pipe
        close(tab->stderr_pipe[0]); // Close read end of stderr pipe
        
        // Redirect stdin, stdout, stderr
        dup2(tab->stdin_pipe[0], STDIN_FILENO);
        dup2(tab->stdout_pipe[1], STDOUT_FILENO);
        dup2(tab->stderr_pipe[1], STDERR_FILENO);
        
        // Close original pipe ends
        close(tab->stdin_pipe[0]);
        close(tab->stdout_pipe[1]);
        close(tab->stderr_pipe[1]);
        
        // Execute shell
        char *shell_args[] = {"/bin/bash", NULL};
        execvp(shell_args[0], shell_args);
        
        // If execvp fails, try other shells
        char *fallback_shells[] = {"/bin/sh", "/usr/bin/bash", NULL};
        for (int i = 0; fallback_shells[i]; i++) {
            execvp(fallback_shells[i], shell_args);
        }
        
        // If all shells fail, exit
        fprintf(stderr, "Failed to start shell\n");
        exit(1);
    } else {
        // Parent process
        // Close unused pipe ends
        close(tab->stdin_pipe[0]);  // Close read end of stdin pipe
        close(tab->stdout_pipe[1]); // Close write end of stdout pipe
        close(tab->stderr_pipe[1]); // Close write end of stderr pipe
        
        // Make pipes non-blocking
        fcntl(tab->stdout_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(tab->stderr_pipe[0], F_SETFL, O_NONBLOCK);
        
        printf("Started shell process with PID %d for tab %d\n", tab->shell_pid, tab->id);
    }
    
    return 0;
}

void shell_process_stop(Tab *tab) {
    if (tab->shell_pid > 0) {
        // Send SIGTERM to shell process
        kill(tab->shell_pid, SIGTERM);
        
        // Wait for process to terminate
        int status;
        waitpid(tab->shell_pid, &status, 0);
        
        printf("Shell process %d terminated\n", tab->shell_pid);
        tab->shell_pid = 0;
    }
    
    // Close pipes
    close(tab->stdin_pipe[0]);
    close(tab->stdin_pipe[1]);
    close(tab->stdout_pipe[0]);
    close(tab->stdout_pipe[1]);
    close(tab->stderr_pipe[0]);
    close(tab->stderr_pipe[1]);
}

int shell_process_write(Tab *tab, const char *data, int len) {
    if (tab->shell_pid <= 0) {
        return -1;
    }
    
    int written = write(tab->stdin_pipe[1], data, len);
    if (written == -1) {
        if (errno == EPIPE) {
            printf("Shell process %d pipe broken\n", tab->shell_pid);
            return -1;
        }
        perror("write to shell");
        return -1;
    }
    
    return written;
}

int shell_process_read(Tab *tab, char *buffer, int max_len) {
    if (tab->shell_pid <= 0) {
        return -1;
    }
    
    int bytes_read = read(tab->stdout_pipe[0], buffer, max_len - 1);
    if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available
        }
        perror("read from shell");
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    return bytes_read;
}

void shell_process_handle_output(MyTermApp *app, Tab *tab) {
    char buffer[1024];
    int bytes_read = shell_process_read(tab, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
        // Null-terminate the buffer
        buffer[bytes_read] = '\0';
        
        // Process output line by line
        char *line_start = buffer;
        char *line_end;
        
        while ((line_end = strchr(line_start, '\n')) != NULL) {
            *line_end = '\0';
            
            // Add line to text buffer (including empty lines for proper formatting)
            text_buffer_add_line(&app->text_buffer, line_start);
            
            line_start = line_end + 1;
        }
        
        // Handle remaining characters (if any)
        if (line_start < buffer + bytes_read) {
            text_buffer_add_line(&app->text_buffer, line_start);
        }
        
        // Redraw the terminal
        x11_redraw(app);
    }
}
