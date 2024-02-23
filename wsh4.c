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
    //const char *path = "/bin"; // Initial PATH contains /bin

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

            int arg_count = 0;
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

        } else {
                // Check if the user typed "exit" to quit
                if (strcmp(job.processes[0].command, "exit") == 0) {
                    free(input_line);
                    freeJob(&job);
                    exit(0);
                } else if (strcmp(job.processes[0].command, "cd") == 0) {
                    if (job.processes[0].args[1] != NULL) {
                        fprintf(stderr, "Usage: cd <directory>\n");
                    } else {
                        if (chdir(job.processes[0].args[0]) != 0) {
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
                            setpgid(0, getpgrp());

                            // Execute the command with its arguments directly
                            execvp(executable_path, job.processes[0].args);

                            // If execvp fails, report an error and exit
                            perror("Command execution failed");
                            exit(1);
                        } else {
                            // Parent process
                            setpgid(pid, getpgrp());
                        }
                    
                }
        }

    }
    free(input_line);
    return 0;
}


            // printf("WE ARE NOT PIPING\n");
            // printf("Number of processes: %d\n", job.process_count);
            // printf("COMMAD: %s\n", job.processes[0].command);
            // printf("Arguments:\n");
            // for (int j = 0; job.processes[0].args[j] != NULL; j++) {
            //     printf("  %s\n", job.processes[0].args[j]);
            // }