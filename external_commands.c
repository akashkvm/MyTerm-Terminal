#include "myterm.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wordexp.h>
#include <time.h>

int parse_command(const char *command, char **argv, int max_args) {
    int argc = 0;
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return -1;
    
    // Tokenize the command
    char *token = strtok(cmd_copy, " \t\n");
    while (token && argc < max_args - 1) {
        argv[argc] = strdup(token);
        argc++;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
    
    free(cmd_copy);
    return argc;
}

int parse_command_with_redirection(const char *command, char **argv, int max_args, char **input_file, char **output_file) {
    int argc = 0;
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return -1;
    
    // Initialize output parameters
    *input_file = NULL;
    *output_file = NULL;
    
    // Check for both input and output redirection
    char *input_redirect = strstr(cmd_copy, " < ");
    char *output_redirect = strstr(cmd_copy, " > ");
    
    if (!input_redirect && !output_redirect) {
        // No redirection, use normal parsing
        free(cmd_copy);
        return parse_command(command, argv, max_args);
    }
    
    // Handle combined redirection (both < and >)
    if (input_redirect && output_redirect) {
        // Determine order: input first or output first
        if (input_redirect < output_redirect) {
            // Format: command < input > output
            *input_redirect = '\0';
            char *input_part = input_redirect + 3; // Skip " < "
            char *output_part = strstr(input_part, " > ");
            if (output_part) {
                *output_part = '\0';
                char *output_file_part = output_part + 3; // Skip " > "
                
                // Parse command part
                char *token = strtok(cmd_copy, " \t\n");
                while (token && argc < max_args - 1) {
                    argv[argc] = strdup(token);
                    argc++;
                    token = strtok(NULL, " \t\n");
                }
                argv[argc] = NULL;
                
                // Parse input file
                char *input_token = strtok(input_part, " \t\n");
                if (input_token) {
                    *input_file = strdup(input_token);
                }
                
                // Parse output file
                char *output_token = strtok(output_file_part, " \t\n");
                if (output_token) {
                    *output_file = strdup(output_token);
                }
            }
        } else {
            // Format: command > output < input
            *output_redirect = '\0';
            char *output_part = output_redirect + 3; // Skip " > "
            char *input_part = strstr(output_part, " < ");
            if (input_part) {
                *input_part = '\0';
                char *input_file_part = input_part + 3; // Skip " < "
                
                // Parse command part
                char *token = strtok(cmd_copy, " \t\n");
                while (token && argc < max_args - 1) {
                    argv[argc] = strdup(token);
                    argc++;
                    token = strtok(NULL, " \t\n");
                }
                argv[argc] = NULL;
                
                // Parse output file
                char *output_token = strtok(output_part, " \t\n");
                if (output_token) {
                    *output_file = strdup(output_token);
                }
                
                // Parse input file
                char *input_token = strtok(input_file_part, " \t\n");
                if (input_token) {
                    *input_file = strdup(input_token);
                }
            }
        }
    }
    // Handle input redirection only
    else if (input_redirect) {
        *input_redirect = '\0';
        char *file_part = input_redirect + 3; // Skip " < "
        
        // Parse the command part (before <)
        char *token = strtok(cmd_copy, " \t\n");
        while (token && argc < max_args - 1) {
            argv[argc] = strdup(token);
            argc++;
            token = strtok(NULL, " \t\n");
        }
        argv[argc] = NULL;
        
        // Parse the input file (after <)
        char *file_token = strtok(file_part, " \t\n");
        if (file_token) {
            *input_file = strdup(file_token);
        }
    }
    // Handle output redirection only
    else if (output_redirect) {
        *output_redirect = '\0';
        char *file_part = output_redirect + 3; // Skip " > "
        
        // Parse the command part (before >)
        char *token = strtok(cmd_copy, " \t\n");
        while (token && argc < max_args - 1) {
            argv[argc] = strdup(token);
            argc++;
            token = strtok(NULL, " \t\n");
        }
        argv[argc] = NULL;
        
        // Parse the output file (after >)
        char *file_token = strtok(file_part, " \t\n");
        if (file_token) {
            *output_file = strdup(file_token);
        }
    }
    
    free(cmd_copy);
    return argc;
}

int is_builtin_command(const char *command) {
    // Extract the first word (command name)
    char cmd_copy[MAX_LINE_LENGTH];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    char *first_word = strtok(cmd_copy, " \t\n");
    if (!first_word) return 0;
    
    // List of built-in commands
    if (strcmp(first_word, "cd") == 0) return 1;
    if (strcmp(first_word, "exit") == 0) return 1;
    if (strcmp(first_word, "pwd") == 0) return 1;
    if (strcmp(first_word, "echo") == 0) return 1;
    if (strcmp(first_word, "multiWatch") == 0) return 1;
    if (strcmp(first_word, "history") == 0) return 1;
    
    return 0;
}

int execute_cd_command(Tab *tab, const char *path) {
    char *target_path = NULL;
    
    if (!path || strlen(path) == 0) {
        // cd with no arguments - go to home directory
        target_path = getenv("HOME");
        if (!target_path) {
            target_path = "/";
        }
    } else if (strcmp(path, "~") == 0) {
        // cd ~ - go to home directory
        target_path = getenv("HOME");
        if (!target_path) {
            target_path = "/";
        }
    } else if (path[0] == '~') {
        // cd ~/path - expand tilde
        const char *home = getenv("HOME");
        if (home) {
            target_path = malloc(strlen(home) + strlen(path));
            if (target_path) {
                strcpy(target_path, home);
                strcat(target_path, path + 1); // Skip the ~
            }
        } else {
            target_path = strdup(path + 1); // Skip the ~
        }
    } else {
        target_path = strdup(path);
    }
    
    if (!target_path) {
        return -1;
    }
    
    int result = chdir(target_path);
    free(target_path);
    
    return result;
}

int execute_builtin_command(MyTermApp *app, Tab *tab, const char *command) {
    char cmd_copy[MAX_LINE_LENGTH];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    char *first_word = strtok(cmd_copy, " \t\n");
    if (!first_word) return -1;
    
    if (strcmp(first_word, "cd") == 0) {
        char *path = strtok(NULL, " \t\n");
        int result = execute_cd_command(tab, path);
        
        if (result == 0) {
            // Success - show current directory
            char cwd[MAX_LINE_LENGTH];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                char output[MAX_LINE_LENGTH + 10];
                snprintf(output, sizeof(output), "%s", cwd);
                text_buffer_add_line(&app->text_buffer, output);
                x11_redraw(app);
            }
        } else {
            // Error - show error message
            char error_msg[MAX_LINE_LENGTH + 20];
            if (path) {
                snprintf(error_msg, sizeof(error_msg), "myterm: cd: %s: No such file or directory", path);
            } else {
                snprintf(error_msg, sizeof(error_msg), "myterm: cd: No such file or directory");
            }
            text_buffer_add_line(&app->text_buffer, error_msg);
            x11_redraw(app);
        }
        return 0;
    } else if (strcmp(first_word, "exit") == 0) {
        app->running = 0;
        return 0;
    } else if (strcmp(first_word, "pwd") == 0) {
        char cwd[MAX_LINE_LENGTH];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char output[MAX_LINE_LENGTH + 10];
            snprintf(output, sizeof(output), "%s\n", cwd);
            text_buffer_add_line(&app->text_buffer, output);
            x11_redraw(app);
        }
        return 0;
    } else if (strcmp(first_word, "echo") == 0) {
        char *args = strtok(NULL, "");
        if (args) {
            // Skip leading whitespace
            while (*args == ' ' || *args == '\t') args++;
            char output[MAX_LINE_LENGTH + 10];
            snprintf(output, sizeof(output), "%s\n", args);
            text_buffer_add_line(&app->text_buffer, output);
            x11_redraw(app);
        } else {
            text_buffer_add_line(&app->text_buffer, "\n");
            x11_redraw(app);
        }
        return 0;
    } else if (strcmp(first_word, "multiWatch") == 0) {
        return execute_multiwatch_command(app, tab, command);
    } else if (strcmp(first_word, "history") == 0) {
        return execute_history_command(app, tab, command);
    }
    
    return -1; // Not a built-in command
}

int execute_external_command(MyTermApp *app, Tab *tab, const char *command) {
    // Check for pipe commands first
    if (strstr(command, "|") != NULL) {
        return execute_pipe_command(app, tab, command);
    }
    
    // Check for input or output redirection
    if (strstr(command, " < ") != NULL || strstr(command, " > ") != NULL) {
        return execute_command_with_input_redirection(app, tab, command);
    }
    
    // Send command to the shell process via stdin pipe
    if (tab->shell_pid > 0 && tab->stdin_pipe[1] != -1) {
        // Add newline to the command
        char cmd_with_newline[MAX_LINE_LENGTH + 2];
        snprintf(cmd_with_newline, sizeof(cmd_with_newline), "%s\n", command);
        
        // Write command to shell stdin
        ssize_t written = write(tab->stdin_pipe[1], cmd_with_newline, strlen(cmd_with_newline));
        if (written == -1) {
            perror("write to shell stdin");
            return -1;
        }
        
        return 0; // Command sent to shell, output will be handled by main loop
    }
    
    return -1; // No active shell process
}

int execute_command_with_input_redirection(MyTermApp *app, Tab *tab, const char *command) {
    char *argv[MAX_LINE_LENGTH];
    char *input_file = NULL;
    char *output_file = NULL;
    
    // Parse command with redirection
    int argc = parse_command_with_redirection(command, argv, MAX_LINE_LENGTH, &input_file, &output_file);
    if (argc <= 0) {
        // Free allocated memory
        for (int i = 0; i < argc; i++) {
            free(argv[i]);
        }
        free(input_file);
        free(output_file);
        return -1;
    }
    
    // Fork a new process for this command
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        
        // Handle input redirection
        if (input_file) {
            int input_fd = open(input_file, O_RDONLY);
            if (input_fd < 0) {
                char error_msg[MAX_LINE_LENGTH + 50];
                snprintf(error_msg, sizeof(error_msg), "myterm: %s: No such file or directory", input_file);
                text_buffer_add_line(&app->text_buffer, error_msg);
                x11_redraw(app);
                exit(EXIT_FAILURE);
            }
            
            // Redirect stdin to the input file
            if (dup2(input_fd, STDIN_FILENO) == -1) {
                perror("dup2");
                close(input_fd);
                exit(EXIT_FAILURE);
            }
            close(input_fd);
        }
        
        // Handle output redirection
        if (output_file) {
            int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd < 0) {
                char error_msg[MAX_LINE_LENGTH + 50];
                snprintf(error_msg, sizeof(error_msg), "myterm: cannot open output file");
                text_buffer_add_line(&app->text_buffer, error_msg);
                x11_redraw(app);
                exit(EXIT_FAILURE);
            }
            
            // Redirect stdout to the output file
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(output_fd);
                exit(EXIT_FAILURE);
            }
            close(output_fd);
        }
        
        // Execute the command
        execvp(argv[0], argv);
        
        // If execvp fails
        char error_msg[MAX_LINE_LENGTH + 50];
        snprintf(error_msg, sizeof(error_msg), "myterm: %s: command not found", argv[0]);
        text_buffer_add_line(&app->text_buffer, error_msg);
        x11_redraw(app);
        exit(EXIT_FAILURE);
        
    } else if (pid > 0) {
        // Parent process - wait for child to complete
        int status;
        waitpid(pid, &status, 0);
        
        // Free allocated memory
        for (int i = 0; i < argc; i++) {
            free(argv[i]);
        }
        free(input_file);
        free(output_file);
        
        return 0;
    } else {
        // Fork failed
        perror("fork");
        
        // Free allocated memory
        for (int i = 0; i < argc; i++) {
            free(argv[i]);
        }
        free(input_file);
        free(output_file);
        
        return -1;
    }
}

// Parse pipe command into individual commands
int parse_pipe_command(const char *command, char **commands, int max_commands) {
    int cmd_count = 0;
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return -1;
    
    // Split by pipe symbol
    char *token = strtok(cmd_copy, "|");
    while (token && cmd_count < max_commands) {
        // Trim whitespace
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) end--;
        *(end + 1) = '\0';
        
        if (strlen(token) > 0) {
            commands[cmd_count] = strdup(token);
            cmd_count++;
        }
        token = strtok(NULL, "|");
    }
    
    free(cmd_copy);
    return cmd_count;
}

// Execute a pipe command
int execute_pipe_command(MyTermApp *app, Tab *tab, const char *command) {
    char *commands[MAX_LINE_LENGTH];
    int num_commands = parse_pipe_command(command, commands, MAX_LINE_LENGTH);
    
    if (num_commands <= 0) {
        // Free allocated memory
        for (int i = 0; i < num_commands; i++) {
            free(commands[i]);
        }
        return -1;
    }
    
    // If only one command, execute normally
    if (num_commands == 1) {
        int result = execute_external_command(app, tab, commands[0]);
        free(commands[0]);
        return result;
    }
    
    // Create pipes for communication between commands
    int pipes[num_commands - 1][2];
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("myterm: pipe failed");
            // Clean up already created pipes
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Free command memory
            for (int j = 0; j < num_commands; j++) {
                free(commands[j]);
            }
            return -1;
        }
    }
    
    // Create a pipe to capture the final output
    int output_pipe[2];
    if (pipe(output_pipe) == -1) {
        perror("myterm: output pipe failed");
        // Clean up command pipes
        for (int i = 0; i < num_commands - 1; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
        // Free command memory
        for (int i = 0; i < num_commands; i++) {
            free(commands[i]);
        }
        return -1;
    }
    
    // Fork processes for each command
    pid_t pids[num_commands];
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("myterm: fork failed");
            // Clean up pipes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            close(output_pipe[0]);
            close(output_pipe[1]);
            // Free command memory
            for (int j = 0; j < num_commands; j++) {
                free(commands[j]);
            }
            return -1;
        }
        
        if (pids[i] == 0) {
            // Child process
            // Redirect stdin from previous pipe (if not first command)
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
                    perror("myterm: dup2 stdin failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Redirect stdout to next pipe or output pipe
            if (i < num_commands - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("myterm: dup2 stdout failed");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Last command - redirect to output pipe
                if (dup2(output_pipe[1], STDOUT_FILENO) == -1) {
                    perror("myterm: dup2 output pipe failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close all pipe ends in child
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            close(output_pipe[0]);
            close(output_pipe[1]);
            
            // Parse and execute the command
            char *argv[MAX_LINE_LENGTH];
            int argc = parse_command(commands[i], argv, MAX_LINE_LENGTH);
            
            if (argc > 0) {
                execvp(argv[0], argv);
                // If execvp fails
                char error_msg[MAX_LINE_LENGTH + 50];
                snprintf(error_msg, sizeof(error_msg), "myterm: %s: command not found", argv[0]);
                text_buffer_add_line(&app->text_buffer, error_msg);
                x11_redraw(app);
            }
            
            // Free command memory in child
            for (int j = 0; j < argc; j++) {
                free(argv[j]);
            }
            exit(EXIT_FAILURE);
        }
    }
    
    // Parent process - close all pipe ends except output pipe read end
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    close(output_pipe[1]); // Close write end, keep read end
    
    // Wait for all child processes to complete
    for (int i = 0; i < num_commands; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }
    
    // Read output from the final command
    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(output_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Process output line by line
        char *line_start = buffer;
        char *line_end;
        
        while ((line_end = strchr(line_start, '\n')) != NULL) {
            *line_end = '\0';
            text_buffer_add_line(&app->text_buffer, line_start);
            line_start = line_end + 1;
        }
        
        // Handle remaining characters (if any)
        if (line_start < buffer + bytes_read) {
            text_buffer_add_line(&app->text_buffer, line_start);
        }
    }
    
    close(output_pipe[0]);
    
    // Free command memory
    for (int i = 0; i < num_commands; i++) {
        free(commands[i]);
    }
    
    // Redraw the terminal
    x11_redraw(app);
    
    return 0;
}

// Parse multiWatch command arguments from bracket notation
int parse_multiwatch_commands(const char *command, char **commands, int max_commands) {
    int cmd_count = 0;
    char *cmd_copy = strdup(command);
    if (!cmd_copy) return -1;
    
    // Find the opening bracket
    char *bracket_start = strchr(cmd_copy, '[');
    if (!bracket_start) {
        free(cmd_copy);
        return -1;
    }
    
    // Find the closing bracket
    char *bracket_end = strrchr(bracket_start, ']');
    if (!bracket_end) {
        free(cmd_copy);
        return -1;
    }
    
    // Extract content between brackets
    *bracket_end = '\0';
    char *content = bracket_start + 1;
    
    // Parse commands separated by commas
    char *token = strtok(content, ",");
    while (token && cmd_count < max_commands) {
        // Trim whitespace and quotes
        while (*token == ' ' || *token == '\t' || *token == '"') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t' || *end == '"')) end--;
        *(end + 1) = '\0';
        
        if (strlen(token) > 0) {
            commands[cmd_count] = strdup(token);
            cmd_count++;
        }
        token = strtok(NULL, ",");
    }
    
    free(cmd_copy);
    return cmd_count;
}

// Global variables for multiWatch signal handling
static pid_t *multiwatch_pids = NULL;
static char **multiwatch_temp_files = NULL;
static int multiwatch_count = 0;
static int multiwatch_running = 0;

// Signal handler for multiWatch
void multiwatch_signal_handler(int sig) {
    if (multiwatch_running && multiwatch_pids && multiwatch_temp_files) {
        // Kill all child processes
        for (int i = 0; i < multiwatch_count; i++) {
            if (multiwatch_pids[i] > 0) {
                kill(multiwatch_pids[i], SIGKILL);
            }
        }
        
        // Clean up temporary files
        for (int i = 0; i < multiwatch_count; i++) {
            if (multiwatch_temp_files[i]) {
                remove(multiwatch_temp_files[i]);
            }
        }
        
        // Free memory
        free(multiwatch_pids);
        free(multiwatch_temp_files);
        multiwatch_pids = NULL;
        multiwatch_temp_files = NULL;
        multiwatch_count = 0;
        multiwatch_running = 0;
    }
    
    // Restore default signal handler
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

// Execute multiWatch command
int execute_multiwatch_command(MyTermApp *app, Tab *tab, const char *command) {
    char *commands[MAX_LINE_LENGTH];
    int num_commands = parse_multiwatch_commands(command, commands, MAX_LINE_LENGTH);
    
    if (num_commands <= 0) {
        char error_msg[MAX_LINE_LENGTH + 50];
        snprintf(error_msg, sizeof(error_msg), "myterm: multiWatch: invalid command format. Use: multiWatch [\"cmd1\", \"cmd2\", ...]");
        text_buffer_add_line(&app->text_buffer, error_msg);
        x11_redraw(app);
        return -1;
    }
    
    // Allocate memory for process management
    multiwatch_pids = malloc(num_commands * sizeof(pid_t));
    multiwatch_temp_files = malloc(num_commands * sizeof(char*));
    multiwatch_count = num_commands;
    multiwatch_running = 1;
    
    if (!multiwatch_pids || !multiwatch_temp_files) {
        perror("myterm: multiWatch: memory allocation failed");
        if (multiwatch_pids) free(multiwatch_pids);
        if (multiwatch_temp_files) free(multiwatch_temp_files);
        multiwatch_pids = NULL;
        multiwatch_temp_files = NULL;
        multiwatch_count = 0;
        multiwatch_running = 0;
        return -1;
    }
    
    // Set up signal handler
    signal(SIGINT, multiwatch_signal_handler);
    
    // Fork processes for each command
    for (int i = 0; i < num_commands; i++) {
        multiwatch_pids[i] = fork();
        
        if (multiwatch_pids[i] == -1) {
            perror("myterm: multiWatch: fork failed");
            // Clean up already forked processes
            for (int j = 0; j < i; j++) {
                kill(multiwatch_pids[j], SIGKILL);
                if (multiwatch_temp_files[j]) {
                    remove(multiwatch_temp_files[j]);
                }
            }
            // Free memory
            free(multiwatch_pids);
            free(multiwatch_temp_files);
            multiwatch_pids = NULL;
            multiwatch_temp_files = NULL;
            multiwatch_count = 0;
            multiwatch_running = 0;
            return -1;
        }
        
        if (multiwatch_pids[i] == 0) {
            // Child process
            // Create temporary file for output
            char temp_filename[50];
            snprintf(temp_filename, sizeof(temp_filename), ".temp.%d.txt", getpid());
            multiwatch_temp_files[i] = strdup(temp_filename);
            
            // Redirect stdout to temporary file
            int fd = open(temp_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("myterm: multiWatch: failed to create temp file");
                exit(EXIT_FAILURE);
            }
            
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("myterm: multiWatch: dup2 failed");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);
            
            // Parse and execute the command
            char *argv[MAX_LINE_LENGTH];
            int argc = parse_command(commands[i], argv, MAX_LINE_LENGTH);
            
            if (argc > 0) {
                execvp(argv[0], argv);
                // If execvp fails
                char error_msg[MAX_LINE_LENGTH + 50];
                snprintf(error_msg, sizeof(error_msg), "myterm: multiWatch: %s: command not found", argv[0]);
                write(STDERR_FILENO, error_msg, strlen(error_msg));
            }
            
            // Free command memory in child
            for (int j = 0; j < argc; j++) {
                free(argv[j]);
            }
            exit(EXIT_FAILURE);
        } else {
            // Parent process - create temp file name for monitoring
            char temp_filename[50];
            snprintf(temp_filename, sizeof(temp_filename), ".temp.%d.txt", multiwatch_pids[i]);
            multiwatch_temp_files[i] = strdup(temp_filename);
        }
    }
    
    // Parent process - monitor outputs
    int temp_fds[num_commands];
    fd_set read_fds;
    int max_fd = 0;
    struct timeval timeout;
    
    // Initialize file descriptors
    for (int i = 0; i < num_commands; i++) {
        temp_fds[i] = -1;
    }
    
    // Monitor outputs
    while (multiwatch_running) {
        // Check if all processes are still running
        int all_dead = 1;
        for (int i = 0; i < num_commands; i++) {
            if (multiwatch_pids[i] > 0) {
                int status;
                pid_t result = waitpid(multiwatch_pids[i], &status, WNOHANG);
                if (result > 0) {
                    // Process has terminated
                    multiwatch_pids[i] = 0;
                } else {
                    all_dead = 0;
                }
            }
        }
        
        if (all_dead) {
            break; // All processes have terminated
        }
        
        // Try to open files that might not be open yet
        for (int i = 0; i < num_commands; i++) {
            if (temp_fds[i] == -1) {
                temp_fds[i] = open(multiwatch_temp_files[i], O_RDONLY | O_NONBLOCK);
            }
        }
        
        // Set up select for monitoring
        FD_ZERO(&read_fds);
        max_fd = 0;
        
        for (int i = 0; i < num_commands; i++) {
            if (temp_fds[i] != -1) {
                FD_SET(temp_fds[i], &read_fds);
                if (temp_fds[i] > max_fd) {
                    max_fd = temp_fds[i];
                }
            }
        }
        
        if (max_fd > 0) {
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int ret = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
            
            if (ret > 0) {
                for (int i = 0; i < num_commands; i++) {
                    if (temp_fds[i] != -1 && FD_ISSET(temp_fds[i], &read_fds)) {
                        char buffer[1024];
                        ssize_t n = read(temp_fds[i], buffer, sizeof(buffer) - 1);
                        
                        if (n > 0) {
                            buffer[n] = '\0';
                            
                            // Get current timestamp
                            time_t now = time(NULL);
                            
                            // Format output with command name and timestamp
                            char formatted_output[MAX_LINE_LENGTH * 4];
                            snprintf(formatted_output, sizeof(formatted_output), 
                                    "\"%s\" , %ld :\n----------------------------------------------------\n%s----------------------------------------------------", 
                                    commands[i], now, buffer);
                            
                            // Add to text buffer
                            text_buffer_add_line(&app->text_buffer, formatted_output);
                            x11_redraw(app);
                        }
                    }
                }
            }
        } else {
            // No files open yet, sleep briefly
            usleep(100000); // 100ms
        }
    }
    
    // Clean up
    for (int i = 0; i < num_commands; i++) {
        if (temp_fds[i] != -1) {
            close(temp_fds[i]);
        }
        if (multiwatch_temp_files[i]) {
            remove(multiwatch_temp_files[i]);
        }
    }
    
    // Free memory
    free(multiwatch_pids);
    free(multiwatch_temp_files);
    multiwatch_pids = NULL;
    multiwatch_temp_files = NULL;
    multiwatch_count = 0;
    multiwatch_running = 0;
    
    // Restore default signal handler
    signal(SIGINT, SIG_DFL);
    
    // Free command memory
    for (int i = 0; i < num_commands; i++) {
        free(commands[i]);
    }
    
    return 0;
}

// History management functions
#define HISTORY_FILE ".myterm_history"
#define MAX_HISTORY_ENTRIES 10000
#define DISPLAY_HISTORY_ENTRIES 1000

// Save command to history file
void save_command_to_history(const char *cmd) {
    if (!cmd || strlen(cmd) == 0) return;
    
    FILE *file = fopen(HISTORY_FILE, "a");
    if (file) {
        fprintf(file, "%s\n", cmd);
        fclose(file);
        
        // Keep only the last MAX_HISTORY_ENTRIES entries
        trim_history_file();
    }
}

// Trim history file to keep only the last MAX_HISTORY_ENTRIES entries
void trim_history_file(void) {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (!file) return;
    
    // Count total lines
    int total_lines = 0;
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        total_lines++;
    }
    fclose(file);
    
    // If we have more than MAX_HISTORY_ENTRIES, trim the file
    if (total_lines > MAX_HISTORY_ENTRIES) {
        // Read all lines
        char **lines = malloc(total_lines * sizeof(char*));
        if (!lines) return;
        
        file = fopen(HISTORY_FILE, "r");
        if (!file) {
            free(lines);
            return;
        }
        
        int i = 0;
        while (fgets(buffer, sizeof(buffer), file) && i < total_lines) {
            lines[i] = strdup(buffer);
            i++;
        }
        fclose(file);
        
        // Write only the last MAX_HISTORY_ENTRIES lines
        file = fopen(HISTORY_FILE, "w");
        if (file) {
            int start = total_lines - MAX_HISTORY_ENTRIES;
            for (int j = start; j < total_lines; j++) {
                fputs(lines[j], file);
            }
            fclose(file);
        }
        
        // Free memory
        for (int j = 0; j < total_lines; j++) {
            free(lines[j]);
        }
        free(lines);
    }
}

// Execute history command
int execute_history_command(MyTermApp *app, Tab *tab, const char *command) {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (!file) {
        char error_msg[MAX_LINE_LENGTH + 50];
        snprintf(error_msg, sizeof(error_msg), "myterm: history: no history file found");
        text_buffer_add_line(&app->text_buffer, error_msg);
        x11_redraw(app);
        return 0;
    }
    
    // Count total lines
    int total_lines = 0;
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        total_lines++;
    }
    rewind(file);
    
    // Display the last DISPLAY_HISTORY_ENTRIES entries
    int start_line = (total_lines > DISPLAY_HISTORY_ENTRIES) ? 
                     (total_lines - DISPLAY_HISTORY_ENTRIES) : 0;
    
    int current_line = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        if (current_line >= start_line) {
            // Remove newline
            buffer[strcspn(buffer, "\n")] = '\0';
            
            // Format with line number
            char formatted_line[MAX_LINE_LENGTH + 20];
            snprintf(formatted_line, sizeof(formatted_line), "%d %s", current_line + 1, buffer);
            text_buffer_add_line(&app->text_buffer, formatted_line);
        }
        current_line++;
    }
    
    fclose(file);
    x11_redraw(app);
    return 0;
}

// Find exact match in history
char* find_exact_match(const char *term) {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (!file) return NULL;
    
    char buffer[MAX_LINE_LENGTH];
    char *last_match = NULL;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove newline
        buffer[strcspn(buffer, "\n")] = '\0';
        
        if (strstr(buffer, term) != NULL) {
            if (last_match) free(last_match);
            last_match = strdup(buffer);
        }
    }
    
    fclose(file);
    return last_match;
}

// Find and print best substring matches
void find_and_print_best_substring_matches(const char *term, MyTermApp *app) {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (!file) return;
    
    char buffer[MAX_LINE_LENGTH];
    char **matches = NULL;
    int match_count = 0;
    int max_matches = 100;
    int best_length = 0;
    
    // First pass: find the longest substring match
    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        
        char *match = strstr(buffer, term);
        if (match) {
            int match_length = strlen(term);
            if (match_length > best_length && match_length > 2) {
                best_length = match_length;
            }
        }
    }
    
    if (best_length <= 2) {
        fclose(file);
        char no_match_msg[MAX_LINE_LENGTH + 50];
        snprintf(no_match_msg, sizeof(no_match_msg), "No match for search term in history");
        text_buffer_add_line(&app->text_buffer, no_match_msg);
        x11_redraw(app);
        return;
    }
    
    // Second pass: collect all matches with the best length
    rewind(file);
    matches = malloc(max_matches * sizeof(char*));
    
    while (fgets(buffer, sizeof(buffer), file) && match_count < max_matches) {
        buffer[strcspn(buffer, "\n")] = '\0';
        
        char *match = strstr(buffer, term);
        if (match && strlen(term) == best_length) {
            matches[match_count] = strdup(buffer);
            match_count++;
        }
    }
    
    fclose(file);
    
    // Print matches
    for (int i = match_count - 1; i >= 0; i--) {
        text_buffer_add_line(&app->text_buffer, matches[i]);
        free(matches[i]);
    }
    
    free(matches);
    x11_redraw(app);
}
