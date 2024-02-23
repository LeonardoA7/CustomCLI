#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_PROCESSES 10 // Maximum number of processes in a job

// Define a struct to represent a process
struct Process {
    char *command;
    char *args[MAX_INPUT_SIZE];
};

// Define a struct to represent a job containing multiple processes
struct Job {
    struct Process processes[MAX_PROCESSES];
    int process_count;
    int piping_flag;
};

// Function to initialize a Process struct
void initProcess(struct Process *proc) {
    proc->command = NULL;
    for (int i = 0; i < MAX_INPUT_SIZE; i++) {
        proc->args[i] = NULL;
    }
}

// Function to free memory allocated for a Process struct
void freeProcess(struct Process *proc) {
    free(proc->command);
    for (int i = 0; i < MAX_INPUT_SIZE; i++) {
        free(proc->args[i]);
    }
}

// Function to initialize a Job struct
void initJob(struct Job *job) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        initProcess(&(job->processes[i]));
    }
    job->process_count = 0;
}

// Function to free memory allocated for a Job struct
void freeJob(struct Job *job) {
    for (int i = 0; i < job->process_count; i++) {
        freeProcess(&(job->processes[i]));
    }
}

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

        // Split the input line into commands based on pipes "|"
        char *pipe_token;
        char *pipe_rest = input_line;

        struct Job job;
        initJob(&job);
        job.piping_flag = 0; // Initialize the flag to 0

        while ((pipe_token = strsep(&pipe_rest, "|")) != NULL) {
            // Initialize a new Process for this command
            struct Process proc;
            initProcess(&proc);

            char *command = strtok(pipe_token, " "); // Get the command
            proc.command = strdup(command);

            proc.args[0] = command;
            int arg_count = 1;
            while (arg_count < MAX_INPUT_SIZE - 2) { // Leave space for NULL and the command
                char *arg = strtok(NULL, " ");
                if (arg == NULL) {
                    break;
                }
                proc.args[arg_count] = strdup(arg);
                arg_count++;
            }
            // Store the Process struct in the Job struct
            if (job.process_count < MAX_PROCESSES) {
                job.processes[job.process_count++] = proc;
            } else {
                fprintf(stderr, "Maximum number of processes in a job exceeded.\n");
                freeProcess(&proc);
                freeJob(&job);
                break;
            }
            // Set the piping_flag to 1 only when there's more than one process
            if (job.process_count > 1) {
                job.piping_flag = 1;
            }
        }










        
        if(job.piping_flag) {

            int pipes[job.process_count - 1][2]; // Array of pipes
            // Create pipes for each command except the last one
            for (int i = 0; i < job.process_count - 1; i++) {
                if (pipe(pipes[i]) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
            }

            // Iterate through the commands and create child processes
            pid_t child_pid;
            for(int i = 0; i < job.process_count; i++) {

                if ((child_pid = fork()) == -1) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }

                if (child_pid == 0) {

                    // Set up input from the previous process (if not the first process)
                    if (i > 0) {
                        // Redirect stdin to the read end of the previous pipe
                        dup2(pipes[i - 1][0], STDIN_FILENO);
                        close(pipes[i - 1][0]);
                        close(pipes[i - 1][1]);
                    
                    }

                    // Set up output to the next process (if not the last process)
                    if (i < job.process_count - 1) {
                        // Redirect stdout to the write end of the current pipe
                        dup2(pipes[i][1], STDOUT_FILENO);
                        close(pipes[i][0]);
                        close(pipes[i][1]);
                    }

                    // Execute the command
                    execvp(job.processes[i].command, job.processes[i].args);
                    perror("execvp");
                    exit(EXIT_FAILURE);
                } else {
                    
                }
            }

            // Close all pipe file descriptors in the parent process
            for (int i = 0; i < job.process_count - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            // Wait for all child processes to finish
            for (int i = 0; i < job.process_count; i++) {
                    int status;
                    waitpid(child_pid, &status, 0);
            }












        } else {
            // Check if the user typed "exit" to quit
            if (strcmp(job.processes[0].command, "exit") == 0) {
                freeJob(&job);
                exit(0);
            } else if (strcmp(job.processes[0].command, "cd") == 0) {
                if (job.processes[0].args[2] != NULL) {
                    fprintf(stderr, "Usage: cd <directory>\n");
                } else {
                    if (chdir(job.processes[0].args[1]) != 0) {
                        perror("cd");
                    }
                }
            } else {
                // Construct the full path to the executable
                char executable_path[MAX_INPUT_SIZE];
                snprintf(executable_path, sizeof(executable_path), "%s/%s", path, job.processes[0].command);

                int pid = fork();

                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                } else if (pid == 0) {
                    // Child process

                    // Execute the command with its arguments directly
                    execvp(executable_path, job.processes[0].args);

                    // If execvp fails, report an error and exit
                    perror("Command execution failed");
                    exit(1);
                } else {
                    // Parent process
                    int status;
                    waitpid(pid, &status, 0);
                }
            }
        }

    }
    free(input_line);
    return 0;
}

