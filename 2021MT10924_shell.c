#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define INITIAL_ARGS_SIZE 10
#define INITIAL_CMD_SIZE 1024

// History storage and tracking
char **history = NULL; // Command history
int history_count = 0; // Number of commands in history
int history_size = 0;  // Capacity of history array

// Previous directory for 'cd -' command
char prev_dir[INITIAL_CMD_SIZE] = ""; // Stores the previous directory

// Add command to history
void add_to_history(const char *cmd) {
    // Resize history array if necessary
    if (history_count >= history_size) {
        history_size = history_size == 0 ? 100 : history_size * 2;
        char **new_history = realloc(history, history_size * sizeof(char *));
        if (new_history == NULL) {
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        }
        history = new_history;
    }
    history[history_count] = strdup(cmd); // Duplicate the command string
    if (history[history_count] == NULL) {
        printf("Invalid Command\n");
        exit(EXIT_FAILURE);
    }
    history_count++;
}

// Print the command history
void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%s\n", history[i]);
    }
}

// Parse command into arguments
void parse_arguments(char *cmd, char ***args, int *args_count, int *args_size) {
    *args_count = 0;
    *args = malloc(*args_size * sizeof(char *)); // Allocate memory for arguments
    if (*args == NULL) {
        printf("Invalid Command\n");
        exit(EXIT_FAILURE);
    }

    char *token = strtok(cmd, " \n"); // Split command by spaces and newlines
    while (token != NULL) {
        // Resize arguments array if necessary
        if (*args_count >= *args_size) {
            *args_size *= 2;
            *args = realloc(*args, *args_size * sizeof(char *));
            if (*args == NULL) {
                printf("Invalid Command\n");
                exit(EXIT_FAILURE);
            }
        }

        // Handle quoted strings
        if (token[0] == '"') {
            char *end_quote = strchr(token + 1, '"');
            if (end_quote != NULL) {
                *end_quote = '\0'; // Null-terminate the quoted string
                (*args)[*args_count] = strdup(token + 1); // Skip the opening quote
                if ((*args)[*args_count] == NULL) {
                    printf("Invalid Command\n");
                    exit(EXIT_FAILURE);
                }
                (*args_count)++;
                token = strtok(NULL, " \n");
            } else {
                // If no ending quote is found, treat the rest of the string as part of the argument
                (*args)[*args_count] = strdup(token);
                if ((*args)[*args_count] == NULL) {
                    printf("Invalid Command\n");
                    exit(EXIT_FAILURE);
                }
                (*args_count)++;
                break;
            }
        } else {
            (*args)[*args_count] = strdup(token);
            if ((*args)[*args_count] == NULL) {
                printf("Invalid Command\n");
                exit(EXIT_FAILURE);
            }
            (*args_count)++;
            token = strtok(NULL, " \n");
        }
    }
    (*args)[*args_count] = NULL; // Null-terminate the arguments array
}

// Execute the given command
void run_command(char *cmd) {
    char **args = NULL;
    int args_size = INITIAL_ARGS_SIZE;
    int args_count = 0;
    char *pipe_pos = strchr(cmd, '|'); // Check for pipes
    int pipe_fd[2];
    int in_fd = 0; // Input file descriptor
    int status;

    while (pipe_pos) {
        *pipe_pos = '\0'; // Split the command at the pipe

        if (pipe(pipe_fd) == -1) {
            printf("Invalid Command\n");
            return;
        }

        if (fork() == 0) {
            dup2(in_fd, STDIN_FILENO); // Set input for the child process
            dup2(pipe_fd[1], STDOUT_FILENO); // Set output for the child process
            close(pipe_fd[0]);

            // Parse and execute the command before the pipe
            args = malloc(args_size * sizeof(char *));
            args_count = 0;

            args[args_count++] = strtok(cmd, " \n");
            while ((args[args_count++] = strtok(NULL, " \n")) != NULL) {
                if (args_count >= args_size) {
                    args_size *= 2;
                    args = realloc(args, args_size * sizeof(char *));
                    if (args == NULL) {
                        printf("Invalid Command\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            args[args_count - 1] = NULL; // Null-terminate the arguments

            if (strcmp(args[0], "history") == 0) {
                // Handle 'history' command with output to pipe
                print_history();
                exit(EXIT_SUCCESS);
            }

            execvp(args[0], args);
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        }

        close(pipe_fd[1]); // Close write end of the pipe in the parent
        wait(&status); // Wait for the child process to finish
        in_fd = pipe_fd[0]; // Set up input for the next command
        cmd = pipe_pos + 1; // Move to the next part of the command
        pipe_pos = strchr(cmd, '|'); // Check for additional pipes
    }

    // Execute the last command
    args = malloc(args_size * sizeof(char *));
    args_count = 0;

    args[args_count++] = strtok(cmd, " \n");
    while ((args[args_count++] = strtok(NULL, " \n")) != NULL) {
        if (args_count >= args_size) {
            args_size *= 2;
            args = realloc(args, args_size * sizeof(char *));
            if (args == NULL) {
                printf("Invalid Command\n");
                free(args);
                return;
            }
        }
    }
    args[args_count - 1] = NULL; // Null-terminate the arguments

    if (strcmp(args[0], "cd") == 0) {
        // Handle 'cd' command
        char current_dir[INITIAL_CMD_SIZE];
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            printf("Invalid Command\n");
            free(args);
            return;
        }

        if (args[1] == NULL || strcmp(args[1], "~") == 0) {
            char *home_dir = getenv("HOME");
            if (home_dir) {
                if (chdir(home_dir) != 0) {
                    printf("Invalid Command\n");
                } else {
                    strncpy(prev_dir, current_dir, sizeof(prev_dir)); // Update previous directory
                }
            } else {
                printf("Invalid Command\n");
            }
        } else if (strcmp(args[1], "-") == 0) {
            if (strlen(prev_dir) == 0) {
                // Do nothing if OLDPWD is not set
            } else {
                if (chdir(prev_dir) == 0) {
                    // On success, print the previous directory
                    printf("%s\n", prev_dir);
                    strncpy(prev_dir, current_dir, sizeof(prev_dir)); // Update previous directory
                } else {
                    printf("Invalid Command\n");
                }
            }
        } else {
            if (chdir(args[1]) != 0) {
                printf("Invalid Command\n");
            } else {
                strncpy(prev_dir, current_dir, sizeof(prev_dir)); // Update previous directory
            }
        }
    } else if (strcmp(args[0], "history") == 0) {
        // Handle 'history' command
        if (in_fd != 0) {
            // Output to pipe
            print_history();
            fflush(stdout);
        } else {
            print_history();
        }
    } else if (strcmp(args[0], "cat") == 0) {
        // Handle 'cat' command
        if (args[1] == NULL) {
            printf("Invalid Command\n");
            free(args);
            return;
        }

        FILE *file = fopen(args[1], "r");
        if (file == NULL) {
            perror("cat"); // Print the standard error message
            free(args);
            return;
        }

        char ch;
        while ((ch = fgetc(file)) != EOF) {
            putchar(ch);
        }
        fclose(file);
        putchar('\n'); // Add newline after file content
    } else if (strcmp(args[0], "dd") == 0) {
        // Handle 'dd' command
        if (args[1] == NULL || strncmp(args[1], "if=", 3) != 0 || args[2] == NULL || strncmp(args[2], "of=", 3) != 0) {
            printf("Invalid Command\n");
            free(args);
            return;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Redirect stdout and stderr to /dev/null
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);

            // Execute the dd command
            execvp(args[0], args);

            // If execvp fails, print error and exit
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            waitpid(pid, &status, 0);
            // Check if the child process exited with an error
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("Invalid Command\n");
            }
        } else {
            printf("Invalid Command\n");
        }
    } else if (strcmp(args[0], "grep") == 0) {
        // Handle 'grep' command
        if (args[1] == NULL) {
            // grep needs at least one pattern and one file or input
            printf("Invalid Command\n");
            free(args);
            return;
        }

        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, STDIN_FILENO); // Final input redirection
            execvp(args[0], args);
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            close(in_fd); // Close the pipe's input side in the parent
            waitpid(pid, &status, 0);
            // Check if the child process exited with an error
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                // If grep exits with status 1 (no matches), don't print "Invalid Command"
                if (WEXITSTATUS(status) != 1) {
                    printf("Invalid Command\n");
                }
            }
        } else {
            printf("Invalid Command\n");
        }
    } else if (strcmp(args[0], "ls") == 0) {
        // Handle 'ls' command
        int stderr_fd[2];
        pipe(stderr_fd);
        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, STDIN_FILENO); // Final input redirection
            dup2(stderr_fd[1], STDERR_FILENO); // Redirect stderr to pipe
            close(stderr_fd[0]);

            execvp(args[0], args);

            // If execvp fails, print error and exit
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            close(stderr_fd[1]);
            char error_buffer[1024];
            ssize_t len = read(stderr_fd[0], error_buffer, sizeof(error_buffer) - 1);
            error_buffer[len] = '\0';

            waitpid(pid, &status, 0);

            // Check if the child process exited with an error
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("Invalid Command\n");
            } else if (strlen(error_buffer) > 0) {
                printf("Invalid Command\n");
            }
        } else {
            printf("Invalid Command\n");
        }
    } else {
        // Handle other commands
        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, STDIN_FILENO); // Final input redirection
            execvp(args[0], args);
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            waitpid(pid, &status, 0);
            // Check if the child process was stopped
            if (WIFSTOPPED(status)) {
                printf("Invalid Command\n");
            }
        } else {
            printf("Invalid Command\n");
        }
    }
    free(args); // Free allocated memory for arguments
}

// Remove leading and trailing spaces from a string
void trim_spaces(char *str) {
    if (str == NULL || *str == '\0') {
        return;
    }

    // Remove leading spaces
    char *start = str;
    while (*start == ' ') {
        start++;
    }

    // If the string is only spaces, return empty
    if (*start == '\0') {
        *str = '\0';
        return;
    }

    // Remove trailing spaces
    char *end = start + strlen(start) - 1;
    while (end > start && *end == ' ') {
        end--;
    }

    // Null terminate the string after the last non-space character
    *(end + 1) = '\0';

    // Move trimmed string to the beginning
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

int main() {
    char *cmd = NULL;
    size_t cmd_size = 0;

    // Initialize history
    history_size = 100;
    history = malloc(history_size * sizeof(char *));
    if (history == NULL) {
        printf("Invalid Command\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("MTL458 > ");
        fflush(stdout);

        ssize_t len = getline(&cmd, &cmd_size, stdin); // Read input command
        if (len == -1) {
            if (feof(stdin)) { // End-of-file (Ctrl+D) should exit
                break;
            } else {
                printf("Invalid Command\n");
                continue;
            }
        }

        // Remove trailing newline and trim spaces
        cmd[strcspn(cmd, "\n")] = '\0';
        trim_spaces(cmd);

        if (strlen(cmd) == 0) {
            continue;
        }

        if (strcmp(cmd, "exit") == 0) {
            break;
        }

        add_to_history(cmd); // Add command to history
        run_command(cmd); // Execute the command
    }

    // Free allocated history memory
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }
    free(history);
    free(cmd);

    return 0;
}
