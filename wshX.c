#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_PROCESSES 15 // Maximum number of processes in a job


// Define a struct to represent a process
struct Process {
    char *command;
    char *args[MAX_INPUT_SIZE];
};

// Define a struct to represent a job containing multiple processes
struct Job {
    int job_id;
    int background;
    struct Process processes[MAX_PROCESSES];
    int process_count;
    int piping_flag;
    char job_string[MAX_INPUT_SIZE * MAX_PROCESSES];
    int pgid;
};

// Declare a global array of 100 Job structs
struct Job *background_jobs[256];
int background_jobs_size = 0;

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
    job->job_id = -1;
    job->background = 0;
    job->piping_flag = 0;
    job->job_string[0] = '\0';
}

// Function to free memory allocated for a Job struct
void freeJob(struct Job *job) {
    for (int i = 0; i < job->process_count; i++) {
        freeProcess(&(job->processes[i]));
    }
    job->process_count = 0;
    job->piping_flag = 0;
    job->job_string[0] = '\0';
}

// Global array to keep track of used job IDs
int used_job_ids[256];

int getSmallestAvailableJobID() {
    for (int i = 1; i <= 256; i++) {
        int found = 0;
        for (int j = 0; j < 256; j++) {
            if (used_job_ids[j] == i) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return i;
        }
    }
    return -1; // Indicates no available job ID (should not happen if MAX_JOBS is sufficient)
}

int largest_job_id = 0;  // Initialize with zero (no job IDs assigned yet)


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
        char *pipe_separator = " | ";
        char *arg_separator = " ";
        char *job_separator = "";
        struct Job job;
        initJob(&job);

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

                // Check if the argument is "&" and skip it
                if (strcmp(arg, "&") == 0) {
                    job.background = 1;
                    continue;
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

            // Append the command and arguments to the job_string
            strcat(job.job_string, job_separator);
            strcat(job.job_string, command);
            for (int i = 0; i < arg_count; i++) {
                strcat(job.job_string, arg_separator);
                strcat(job.job_string, proc.args[i]);
            }


            job_separator = pipe_separator; // Change the separator for the next iteration
        }

        // Find the smallest available job ID
        int smallest_job_id = getSmallestAvailableJobID();
        if (smallest_job_id == -1) {
            fprintf(stderr, "Error: No available job IDs.\n");
            free(input_line);
            exit(1);
        }

        job.job_id = smallest_job_id;
        // Update the largest job ID if the current job ID is larger.
        if (smallest_job_id > largest_job_id) {
            largest_job_id = smallest_job_id;
        }

        job.pgid = getpgid(0);
        // printf("Number of processes: %d\n", job.process_count);
        // printf("JOB ID: %d\n", job.job_id);
        // printf("COMMAD: %s\n", job.processes[0].command);
        // printf("Arguments:\n");
        // for (int j = 0; job.processes[0].args[j] != NULL; j++) {
        //     printf("  %s\n", job.processes[0].args[j]);
        // }

        // We successfully created the job

        // Is it a background job?
        if(job.background) {
            for(int i = 0; i < 100; i++) {
                if(background_jobs[i] == NULL) {
                    background_jobs_size++;
                    background_jobs[i] = &job;
                    break;
                }
            }
                // Construct the full path to the executable
                char executable_path[MAX_INPUT_SIZE];
                snprintf(executable_path, sizeof(executable_path), "%s/%s", path, job.processes[0].command);

                int pid = fork();

                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                } else if (pid == 0) {
                    // Child process
                    setpgid(0, 0);
                    
                    // Execute the command with its arguments directly
                    execvp(executable_path, job.processes[0].args);

                    // If execvp fails, report an error and exit
                    perror("Command execution failed");
                    exit(1);
                } else {
                    // Parent process
                    printf("Background job started with PID: %d\n", pid);
                }
        }

        if(job.piping_flag) {

            int pipe_fds[2];
            int prev_pipe_read = -1; // Initialize to -1 for the first process

            for (int i = 0; i < job.process_count; i++) {
                if (pipe(pipe_fds) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }

                pid_t parent_pid = getpid();
                pid_t pid = fork();

                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                } else if (pid == 0) {
                    // Child process
                        pid_t parent_pgid = getpgid(parent_pid);
                        if (parent_pgid == -1) {
                            perror("getpgid");
                            // Handle the error
                        } else {
                            setpgid(0, parent_pgid); // Set child's pgid to match the parent's
                            // ...
                        }
                    if (i > 0) {
                        // Redirect stdin to the previous pipe's read end
                        dup2(prev_pipe_read, STDIN_FILENO);
                        close(prev_pipe_read);
                    }

                    if (i < job.process_count - 1) {
                        // Redirect stdout to the current pipe's write end
                        dup2(pipe_fds[1], STDOUT_FILENO);
                        close(pipe_fds[0]);
                        close(pipe_fds[1]);
                    }

                    // Execute the command
                    execvp(job.processes[i].command, job.processes[i].args);
                    perror("execvp");
                    exit(EXIT_FAILURE);
                } else {
                    // Parent process
                    if (prev_pipe_read != -1) {
                        close(prev_pipe_read);
                    }

                    close(pipe_fds[1]); // Close the write end of the current pipe
                    prev_pipe_read = pipe_fds[0]; // Update the previous pipe's read end
                }
            }

            // Close the last pipe's read end
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            // Wait for all child processes to finish
            for (int i = 0; i < job.process_count; i++) {
                wait(NULL);
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
            } else if (strcmp(job.processes[0].command, "bg") == 0) {
                if (job.processes[0].args[2] != NULL) {
                    fprintf(stderr, "Usage: bg <id>\n");
                } else {
                    // int temp;
                    // if(job.processes[0].args[1] != NULL) {
                    //     temp = atoi(job.processes[0].args[1]);
                    // } else {
                    //     temp = largest_job_id;
                    // }
                    
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
                    setpgid(0, 0);
                    
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

