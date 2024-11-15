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
#define MAX_TOKENS 100

void print_prompt() {
    printf("mysh> ");
    fflush(stdout);
}

void execute_command(char *cmd);
void change_directory(char **args);
void print_working_directory();
char **tokenize_input(char *input, int *token_count);
void free_tokens(char **tokens, int token_count);
void run_tests();

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "--test") == 0) {
        run_tests();
        return 0;
    }

    char cmd[MAX_CMD_LEN];
    FILE *input = stdin;
    int interactive = isatty(fileno(stdin));

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [batch_file] or %s --test\n", argv[0], argv[0]);
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

        int token_count;
        char **tokens = tokenize_input(cmd, &token_count);

        // Debug: Print tokens
        for (int i = 0; i < token_count; i++) {
            printf("Token %d: %s\n", i, tokens[i]);
        }

        free_tokens(tokens, token_count);
    }

    if (argc == 2) {
        fclose(input);
    }

    return 0;
}
// to test the code type "./mysh --test" afer building executable
void run_tests() {
    char *test_cases[] = {
        "ls -l /home/user",
        "cat file.txt | grep hello",
        "   echo    hello   world  ",
        "cd /tmp",
        "pwd",
        "echo \"This is a test\"",
        NULL
    };

    for (int i = 0; test_cases[i] != NULL; i++) {
        printf("\nTest Case %d: %s\n", i + 1, test_cases[i]);
        int token_count;
        char *test_input = strdup(test_cases[i]);
        char **tokens = tokenize_input(test_input, &token_count);

        for (int j = 0; j < token_count; j++) {
            printf("Token %d: %s\n", j, tokens[j]);
        }

        free(test_input);
        free_tokens(tokens, token_count);
    }
}

char **tokenize_input(char *input, int *token_count) {
    char **tokens = malloc(MAX_TOKENS * sizeof(char *));
    if (!tokens) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int count = 0;
    char *token = strtok(input, " \t\n");
    while (token != NULL && count < MAX_TOKENS) {
        tokens[count++] = strdup(token);
        token = strtok(NULL, " \t\n");
    }
    tokens[count] = NULL;
    *token_count = count;

    return tokens;
}

void free_tokens(char **tokens, int token_count) {
    for (int i = 0; i < token_count; i++) {
        free(tokens[i]);
    }
    free(tokens);
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
