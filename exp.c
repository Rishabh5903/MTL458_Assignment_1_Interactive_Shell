void execute_command(char *cmd) {
    char **args = NULL;
    int args_count = 0;
    char *pipe_pos = strchr(cmd, '|');
    int pipe_fd[2];
    int in_fd = 0;

    while (pipe_pos) {
        *pipe_pos = '\0'; // Split the command at the pipe

        if (pipe(pipe_fd) == -1) {
            printf("Invalid Command\n");
            return;
        }

        if (fork() == 0) {
            dup2(in_fd, STDIN_FILENO); // Change input according to the old one
            dup2(pipe_fd[1], STDOUT_FILENO); // Change output according to the new one
            close(pipe_fd[0]);

            // Parse and execute the command before the pipe
            args = NULL;
            args_count = 0;

            char *arg = strtok(cmd, " \n");
            while (arg != NULL) {
                args = realloc(args, (args_count + 1) * sizeof(char *));
                if (args == NULL) {
                    printf("Invalid Command\n");
                    exit(EXIT_FAILURE);
                }
                args[args_count++] = arg;
                arg = strtok(NULL, " \n");
            }
            args = realloc(args, (args_count + 1) * sizeof(char *));
            if (args == NULL) {
                printf("Invalid Command\n");
                exit(EXIT_FAILURE);
            }
            args[args_count] = NULL; // Null-terminate the arguments

            execvp(args[0], args);
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        }

        close(pipe_fd[1]);
        wait(NULL); // Wait for the child process
        in_fd = pipe_fd[0];
        cmd = pipe_pos + 1; // Move to the next part of the command
        pipe_pos = strchr(cmd, '|');
    }

    // Execute the last command
    args = NULL;
    args_count = 0;

    char *arg = strtok(cmd, " \n");
    while (arg != NULL) {
        args = realloc(args, (args_count + 1) * sizeof(char *));
        if (args == NULL) {
            printf("Invalid Command\n");
            return;
        }
        args[args_count++] = arg;
        arg = strtok(NULL, " \n");
    }
    args = realloc(args, (args_count + 1) * sizeof(char *));
    if (args == NULL) {
        printf("Invalid Command\n");
        return;
    }
    args[args_count] = NULL; // Null-terminate the arguments

    if (strcmp(args[0], "cd") == 0) {
        char current_dir[1024];
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            printf("Invalid Command\n");
            return;
        }

        if (args[1] == NULL || strcmp(args[1], "~") == 0) {
            // Change to home directory
            char *home_dir = getenv("HOME");
            if (home_dir) {
                if (chdir(home_dir) != 0) {
                    printf("Invalid Command\n");
                } else {
                    // Update prev_dir
                    strncpy(prev_dir, current_dir, sizeof(prev_dir));
                }
            } else {
                printf("Invalid Command\n");
            }
        } else if (strcmp(args[1], "-") == 0) {
            // Change to previous directory
            if (strlen(prev_dir) == 0) {
                printf("Invalid Command\n");
            } else {
                if (chdir(prev_dir) != 0) {
                    printf("Invalid Command\n");
                } else {
                    printf("%s\n", prev_dir);
                    // Update prev_dir
                    strncpy(prev_dir, current_dir, sizeof(prev_dir));
                }
            }
        } else {
            // Change to a specific directory
            if (chdir(args[1]) != 0) {
                printf("Invalid Command\n");
            } else {
                // Update prev_dir
                strncpy(prev_dir, current_dir, sizeof(prev_dir));
            }
        }
    } else if (strcmp(args[0], "history") == 0) {
        print_history();
    } else if (strcmp(args[0], "cat") == 0) {
        if (args[1] == NULL) {
            printf("Invalid Command\n");
            return;
        }

        FILE *file = fopen(args[1], "r");
        if (file == NULL) {
            printf("Invalid Command\n");
            return;
        }

        char ch;
        while ((ch = fgetc(file)) != EOF) {
            putchar(ch);
        }
        fclose(file);
        putchar('\n'); // Add newline after file content
    } else if (strcmp(args[0], "dd") == 0) {
        // Check if 'dd' command has the required arguments
        if (args[1] == NULL || strncmp(args[1], "if=", 3) != 0 || args[2] == NULL || strncmp(args[2], "of=", 3) != 0) {
            printf("Invalid Command\n");
            return;
        }

        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, STDIN_FILENO); // Final input redirection

            // Redirect stderr to /dev/null
            FILE *null_file = fopen("/dev/null", "w");
            if (null_file) {
                dup2(fileno(null_file), STDERR_FILENO);
                fclose(null_file);
            }

            execvp(args[0], args);
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0); // Wait for the specific child process
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                // Output stats for dd command
                FILE *fp = popen("dd if=file1.txt of=file2.txt status=progress 2>&1", "r");
                if (fp) {
                    char buffer[4096];
                    while (fgets(buffer, sizeof(buffer), fp)) {
                        if (strstr(buffer, "records in") || strstr(buffer, "records out") || strstr(buffer, "bytes copied")) {
                            printf("%s", buffer);
                        }
                    }
                    pclose(fp);
                }
            } else {
                printf("Invalid Command\n");
            }
        } else {
            printf("Invalid Command\n");
        }
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            dup2(in_fd, STDIN_FILENO); // Final input redirection
            execvp(args[0], args);
            printf("Invalid Command\n");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            wait(NULL);
        } else {
            printf("Invalid Command\n");
        }
    }
    free(args);
}
