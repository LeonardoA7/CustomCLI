#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
// https://gist.github.com/mjkpolo/8647b53a78ec9aa53ffce6d9dc5f3fa8


int main(int argc, char *argv[]) {
    char *input_line = NULL;
    size_t line_size = 0;
    const char *path = "/bin"; // Initial PATH contains /bin

    // Interactive mode
    while (1) {
        printf("wsh> ");

        // Read a line of input
        ssize_t read = getline(&input_line, &line_size, stdin);
        if (read == -1) {
            // End of file (Ctrl-D) or error
            free(input_line);
            exit(0);
        }

        // Remove trailing newline
        if (input_line[read - 1] == '\n') {
            input_line[read - 1] = '\0';
        }

        // Parse input into command and arguments
        char *token;
        char *args[MAX_INPUT_SIZE];
        int arg_count = 0;
        char *rest = input_line;

        while ((token = strsep(&rest, " ")) != NULL) {
            args[arg_count] = token;
            arg_count++;
        }

        args[arg_count] = NULL;

        // Fork a new process and execute the command
        int pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // Check if the user typed "exit" to quit
            if (strcmp(args[0], "exit") == 0) {
                free(input_line);
                exit(0);
            } else if (strcmp(args[0], "cd") == 0) {
                if (arg_count != 2) {
                    fprintf(stderr, "Usage: cd <directory>\n");
                } else {
                    if (chdir(args[1]) != 0) {
                        perror("cd");
                    }
                }
            }  else {
                // Construct the full path to the executable
                char executable_path[MAX_INPUT_SIZE];
                snprintf(executable_path, sizeof(executable_path), "%s/%s", path, args[0]);

                execvp(executable_path, args);
                perror("Command execution failed");
                exit(1);
            }
        } else {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
        }
    }

    free(input_line);
    return 0;
}

