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
#include <unistd.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 100
#define MAX_TOKENS 100


typedef struct {
    char **arguments;
    char *execpath;
    char *inputfile;
    char *outputfile;
    char *pipeto;
    int arg_count;
} command_t;


void print_prompt() {
    printf("mysh> ");
    fflush(stdout);
}


void execute_command(command_t *cmd);

void change_directory(char **args);

void print_working_directory();

void print_which(char** args);

char **tokenize_input(char *input, int *token_count);

void free_tokens(char **tokens, int token_count);

command_t *parse_command(char **tokens, int token_count);

void free_command(command_t *cmd);


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
        command_t *parsed_command = parse_command(tokens, token_count);

        if (parsed_command != NULL) {
            execute_command(parsed_command);
            free_command(parsed_command);
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


command_t *parse_command(char **tokens, int token_count){
    command_t *cmd = malloc(sizeof(command_t));


    if (!cmd) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    cmd->arguments = malloc(MAX_ARGS * sizeof(char *));
    cmd->execpath = NULL;
    cmd->inputfile = NULL;
    cmd->outputfile = NULL;
    cmd->pipeto = NULL;
    cmd->arg_count = 0;


    int i = 0;

    while (i < token_count) {
        if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 < token_count) {
                cmd->inputfile = strdup(tokens[i + 1]);
                i += 2;

            } else {
                fprintf(stderr, "Syntax error: expected file after '<'\n");
                free_command(cmd);
                return NULL;

            }
        } else if (strcmp(tokens[i], ">") == 0) {
            if (i + 1 < token_count) {
                cmd->outputfile = strdup(tokens[i + 1]);
                i += 2;

            } else {
                fprintf(stderr, "Syntax error: expected file after '>'\n");
                free_command(cmd);
                return NULL;
            }

        } 

        else if (strcmp(tokens[i], "|") == 0) {
            if (i + 1 < token_count) {
                cmd->pipeto = strdup(tokens[i + 1]);
                i += 2;

        }

        else {
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
            i++;
        }

    }

    cmd->arguments[cmd->arg_count] = NULL;
    cmd->execpath = strdup(cmd->arguments[0]);

    return cmd;
}
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
    // Fork a child process to execute other commands
    pid_t pid = fork();
    pid_t pipepid = -1;
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
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

        if (cmd->pipeto){
            pipepid = fork ();
            int filedes[2];
            dup2(filedes[0], stdout);
            dup2(filedes[1],stdin);;
            pipe(filedes);
            execv(cmd->pipeto, cmd->arguments);
        }

        execv(cmd->execpath, cmd->arguments);

        perror("execv");

        exit(EXIT_FAILURE);

    } else {
        // Parent process
        int status;
        int pipestatus;
        waitpid(pid, &status, 0);

        if (pipepid != -1)
        waitpid(pipepid, &pipestatus, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            printf("Command failed with code %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Terminated by signal: %d\n", WTERMSIG(status));
        }
        if (WIFEXITED(pipestatus) && WEXITSTATUS(pipestatus) != 0) {
            printf("pipe to Command failed with code %d\n", WEXITSTATUS(pipestatus));
        } else if (WIFSIGNALED(pipestatus)) {
            printf("pipe to command Terminated by signal: %d\n", WTERMSIG(pipestatus));
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

void print_which(char **args){
    if (args[1] == NULL) {
        fprintf(stderr, "which: missing argument\n");
        return;
    }
    char* envp = getenv("PATH");

    if (envp){
        char* allpaths = strtok (envp, ":");
        while (allpaths != NULL)

        { 
            int len = strlen (args[0]) + strlen (allpaths) + 2;
            char* fullpathtosearch = (char*) malloc (sizeof (char) * len);
            sprintf (fullpathtosearch, "%s%s%s", (char*) allpaths, "/", args[1]);
            if (access(fullpathtosearch, F_OK) == 0){
                printf ("%s\n", fullpathtosearch);
                fflush (stdout);
            }

            allpaths=strtok (NULL, ":");

            free (fullpathtosearch);

        }

    }

}

