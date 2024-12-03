#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <glob.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 100
#define MAX_TOKENS 100
#define MAX_COMMANDS 10

typedef struct {
    char **arguments;
    char *execpath;
    char *inputfile;
    char *outputfile;
    int arg_count;
} command_t;

void print_prompt() {
    printf("mysh> ");
    fflush(stdout);
}

void execute_pipeline(command_t **commands, int command_count);
void execute_command(command_t *cmd);
void change_directory(char **args);
void print_working_directory();
void print_which(char** args);
char **tokenize_input(char *input, int *token_count);
void free_tokens(char **tokens, int token_count);
command_t *parse_command(char **tokens, int start, int end);
command_t **parse_pipeline(char **tokens, int token_count, int *command_count);
void free_command(command_t *cmd);
char *find_executable(char *cmd_name);

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
        interactive = 0;
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
        int command_count;
        command_t **commands = parse_pipeline(tokens, token_count, &command_count);

        if (commands != NULL) {
            if (command_count == 1) {
                execute_command(commands[0]);
            } else {
                execute_pipeline(commands, command_count);
            }
            for (int i = 0; i < command_count; i++) {
                free_command(commands[i]);
            }
            free(commands);
        }
        free_tokens(tokens, token_count);
    }

    if (argc == 2) {
        fclose(input);
    }

    return 0;
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

command_t *parse_command(char **tokens, int start, int end) {
    command_t *cmd = malloc(sizeof(command_t));

    if (!cmd) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    cmd->arguments = malloc(MAX_ARGS * sizeof(char *));
    cmd->execpath = NULL;
    cmd->inputfile = NULL;
    cmd->outputfile = NULL;
    cmd->arg_count = 0;

    for (int i = start; i <= end; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 <= end) {
                cmd->inputfile = strdup(tokens[i + 1]);
                i++;
            } else {
                fprintf(stderr, "Syntax error: expected file after '<'\n");
                free_command(cmd);
                return NULL;
            }
        } else if (strcmp(tokens[i], ">") == 0) {
            if (i + 1 <= end) {
                cmd->outputfile = strdup(tokens[i + 1]);
                i++;
            } else {
                fprintf(stderr, "Syntax error: expected file after '>'\n");
                free_command(cmd);
                return NULL;
            }
        } else {
            // Handle wildcard expansion
            glob_t globbuf;
            if (strchr(tokens[i], '*')) {
                if (glob(tokens[i], 0, NULL, &globbuf) == 0) {
                    for (size_t j = 0; j < globbuf.gl_pathc; j++) {
                        cmd->arguments[cmd->arg_count++] = strdup(globbuf.gl_pathv[j]);
                    }
                } else {
                    cmd->arguments[cmd->arg_count++] = strdup(tokens[i]);
                }
                globfree(&globbuf);
            } else {
                cmd->arguments[cmd->arg_count++] = strdup(tokens[i]);
            }
        }
    }

    cmd->arguments[cmd->arg_count] = NULL;
    if (cmd->arg_count > 0) {
        if (tokens[start][0] == '/') {
            cmd->execpath = strdup(tokens[start]);
        } else {
            cmd->execpath = find_executable(tokens[start]);
            if (!cmd->execpath) {
                fprintf(stderr, "Command not found: %s\n", tokens[start]);
                free_command(cmd);
                return NULL;
            }
        }
    }

    return cmd;
}

command_t **parse_pipeline(char **tokens, int token_count, int *command_count) {
    command_t **commands = malloc(MAX_COMMANDS * sizeof(command_t *));
    if (!commands) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int start = 0;
    *command_count = 0;

    for (int i = 0; i <= token_count; i++) {
        if (i == token_count || strcmp(tokens[i], "|") == 0) {
            if (*command_count >= MAX_COMMANDS) {
                fprintf(stderr, "Too many commands in pipeline\n");
                for (int j = 0; j < *command_count; j++) {
                    free_command(commands[j]);
                }
                free(commands);
                return NULL;
            }
            commands[*command_count] = parse_command(tokens, start, i - 1);
            if (commands[*command_count] == NULL) {
                for (int j = 0; j < *command_count; j++) {
                    free_command(commands[j]);
                }
                free(commands);
                return NULL;
            }
            (*command_count)++;
            start = i + 1;
        }
    }

    return commands;
}

void free_command(command_t *cmd) {
    for (int i = 0; i < cmd->arg_count; i++) {
        free(cmd->arguments[i]);
    }
    free(cmd->arguments);
    if (cmd->execpath) free(cmd->execpath);
    if (cmd->inputfile) free(cmd->inputfile);
    if (cmd->outputfile) free(cmd->outputfile);
    free(cmd);
}

void execute_pipeline(command_t **commands, int command_count) {
    int pipefds[2 * (command_count - 1)];
    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipefds + 2 * i) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < command_count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Set up input from previous command
            if (i > 0) {
                dup2(pipefds[2 * (i - 1)], STDIN_FILENO);
            }
            // Set up output to next command
            if (i < command_count - 1) {
                dup2(pipefds[2 * i + 1], STDOUT_FILENO);
            }
            // Close all pipe fds
            for (int j = 0; j < 2 * (command_count - 1); j++) {
                close(pipefds[j]);
            }
            // Handle redirections
            if (commands[i]->inputfile) {
                int fd_in = open(commands[i]->inputfile, O_RDONLY);
                if (fd_in < 0) {
                    perror("open input file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if (commands[i]->outputfile) {
                int fd_out = open(commands[i]->outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (fd_out < 0) {
                    perror("open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
            execv(commands[i]->execpath, commands[i]->arguments);
            perror("execv");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipe fds in parent
    for (int i = 0; i < 2 * (command_count - 1); i++) {
        close(pipefds[i]);
    }

    // Wait for all children
    for (int i = 0; i < command_count; i++) {
        int status;
        wait(&status);
    }
}

void execute_command(command_t *cmd) {
    if (cmd->execpath == NULL) {
        return; // Empty command
    }

    // Built-in command: cd
    if (strcmp(cmd->execpath, "cd") == 0) {
        change_directory(cmd->arguments);
        return;
    }

    // Built-in command: pwd
    else if (strcmp(cmd->execpath, "pwd") == 0) {
        print_working_directory();
        return;
    }
    else if (strcmp(cmd->execpath, "which") == 0) {
        print_which(cmd->arguments);
        return;
    }

    // Fork a child process to execute the command
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Handle redirections
        if (cmd->inputfile) {
            int fd_in = open(cmd->inputfile, O_RDONLY);
            if (fd_in < 0) {
                perror("open input file");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if (cmd->outputfile) {
            int fd_out = open(cmd->outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd_out < 0) {
                perror("open output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        execv(cmd->execpath, cmd->arguments);
        perror("execv");
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
    } else if (strcmp(args[1], "..") == 0) {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
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

void print_which(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "which: missing argument\n");
        return;
    }
    char *envp = getenv("PATH");

    if (envp) {
        char *allpaths = strtok(envp, ":");
        while (allpaths != NULL) {
            int len = strlen(args[1]) + strlen(allpaths) + 2;
            char *fullpathtosearch = (char *)malloc(sizeof(char) * len);
            sprintf(fullpathtosearch, "%s/%s", allpaths, args[1]);
            if (access(fullpathtosearch, F_OK) == 0) {
                printf("%s\n", fullpathtosearch);
                fflush(stdout);
            }

            allpaths = strtok(NULL, ":");
            free(fullpathtosearch);
        }
    }
}

char *find_executable(char *cmd_name) {
    // Custom search for executable in 3 specific folders
    char *paths[] = {"/bin", "/usr/bin", "/usr/local/bin"};
    char *fullpath = malloc(256);

    for (int i = 0; i < 3; i++) {
        snprintf(fullpath, 256, "%s/%s", paths[i], cmd_name);
        if (access(fullpath, X_OK) == 0) {
            return fullpath;
        }
    }

    free(fullpath);
    return NULL;
}
