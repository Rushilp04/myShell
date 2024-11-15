#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 100

void print_prompt() {
    printf("mysh> ");
    fflush(stdout);
}

void execute_command(char *cmd);
void change_directory(char **args);
void print_working_directory();

int main(int argc, char *argv[]) {
    char cmd[MAX_CMD_LEN];
    FILE *input = stdin;
    int interactive = isatty(fileno(stdin));

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [batch_file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    while (1) {
        if (interactive) {
            print_prompt();
        }

        if (!fgets(cmd, MAX_CMD_LEN, input)) {
            break;
        }

        // Remove trailing newline
        cmd[strcspn(cmd, "\n")] = 0;

        // Check for "exit" command
        if (strcmp(cmd, "exit") == 0) {
            if (interactive) {
                printf("Exiting my shell.\n");
            }
            break;
        }

        execute_command(cmd);
    }

    if (argc == 2) {
        fclose(input);
    }

    return 0;
}

void execute_command(char *cmd) {
    char *args[MAX_ARGS];
    int i = 0;
    char *token = strtok(cmd, " ");

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (args[0] == NULL) {
        return; // Empty command
    }

    // Built-in command: cd
    if (strcmp(args[0], "cd") == 0) {
        change_directory(args);
        return;
    }

    // Built-in command: pwd
    if (strcmp(args[0], "pwd") == 0) {
        print_working_directory();
        return;
    }

    // Fork a child process to execute other commands
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            printf("Command failed with code %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Terminated by signal: %d\n", WTERMSIG(status));
        }
    }
}

void change_directory(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: missing argument\n");
    } else if (chdir(args[1]) != 0) {
        perror("cd");
    }
}

void print_working_directory() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd");
    }
}
