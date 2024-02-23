#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_PROCESSES 15
#define MAX_JOBS 256

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
    pid_t pgid;
};

// Global array to keep track of used job IDs
int used_job_ids[MAX_JOBS];

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

// Function to find the smallest available job ID
int getSmallestAvailableJobID() {
    for (int i = 1; i < MAX_JOBS; i++) {
        if (used_job_ids[i] == 0) {
            return i;
        }
    }
    return -1; // Indicates no available job ID
}

int largest_job_id = 0; // Initialize with zero (no job IDs assigned yet)

// Declare an array of Job structs to hold background jobs
struct Job *background_jobs[MAX_JOBS];
int background_jobs_size = 0;

// Function to remove a background job from the list by pointer
void removeBackgroundJob(struct Job *job, struct Job *background_jobs[], int *background_jobs_size) {
    // Find the index of the job in the background_jobs array
    int index = -1;
    for (int i = 0; i < *background_jobs_size; i++) {
        if (background_jobs[i] == job) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        // Shift elements to remove the job from the array
        for (int i = index; i < *background_jobs_size - 1; i++) {
            background_jobs[i] = background_jobs[i + 1];
        }

        // Decrease the size of the array
        (*background_jobs_size)--;
    }
}


void printBackgroundJobs() {
    for (int i = 0; i < background_jobs_size; i++) {
        struct Job *job = background_jobs[i];
        printf("%d: ", job->job_id);
        
        for (int j = 0; j < job->process_count; j++) {
            printf("%s", job->processes[j].command);
            
            for (int k = 1; job->processes[j].args[k] != NULL; k++) {
                printf(" %s", job->processes[j].args[k]);
            }
            
            if (j < job->process_count - 1) {
                printf(" | ");
            }
        }
        
        if (job->background) {
            printf(" &");
        }
        
        printf("\n");
    }
}

void addToBackgroundJobs(struct Job* job) {
    if (background_jobs_size < MAX_JOBS) {
        background_jobs[background_jobs_size] = malloc(sizeof(struct Job));
        if (background_jobs[background_jobs_size] == NULL) {
            perror("Memory allocation failed");
            exit(1);
        }
        memcpy(background_jobs[background_jobs_size], job, sizeof(struct Job));
        (background_jobs_size)++;
    } else {
        fprintf(stderr, "Maximum number of background jobs exceeded.\n");
    }
}

int main(int argc, char *argv[]) {
    char *input_line = NULL;
    size_t line_size = 0;
    const char *path = "/bin"; // Initial PATH contains /bin

    // Initialize used_job_ids to mark all job IDs as unused
    for (int i = 0; i < MAX_JOBS; i++) {
        used_job_ids[i] = 0;
    }

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
            // Check if any background jobs have completed
            for (int i = 0; i < background_jobs_size; i++) {
                // printf("CHECKING FOR FINISHED JOBS: \n");
                // printf("BACKGROUND JOB ID: %d\n", background_jobs[0]->pgid);
                int status;
                pid_t result = waitpid(background_jobs[i]->pgid, &status, WNOHANG);
                // printf("PID_T: %d", result);
                if(result == -1) {
                    // The job with PGID background_jobs[i]->pgid has completed
                    printf("Background job with PID %d has completed.\n", result);
                    
                    // Remove the job from the background_jobs list
                    removeBackgroundJob(background_jobs[i], background_jobs, &background_jobs_size);
                    
                    // When a job is removed or no longer in use, reset the corresponding index in used_job_ids
                    used_job_ids[(background_jobs[i]->job_id-1)] = 0;  
                } else if (result > 0) {
                    // The job with PGID background_jobs[i]->pgid has completed
                    // printf("Background job with PID %d has completed.\n", result);
                    
                    // Remove the job from the background_jobs list
                    removeBackgroundJob(background_jobs[i], background_jobs, &background_jobs_size);
                    
                    // When a job is removed or no longer in use, reset the corresponding index in used_job_ids
                    printf("THis should be 3: %d\n", background_jobs[i]->job_id);
                    used_job_ids[(background_jobs[i]->job_id-1)] = 0;
                }
            }
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

        // When assigning a job ID, update the used_job_ids array
        int smallest_job_id = getSmallestAvailableJobID();
        if (smallest_job_id != -1) {
            used_job_ids[smallest_job_id] = 1;
            job.job_id = smallest_job_id;
        } else {
            fprintf(stderr, "Error: No available job IDs.\n");
            free(input_line);
            exit(1);
        }

        job.job_id = smallest_job_id;
        // Update the largest job ID if the current job ID is larger.
        if (smallest_job_id > largest_job_id) {
            largest_job_id = smallest_job_id;
        }

        printf("THIS JOB ID: %d\n", job.job_id);
        job.pgid = getpgid(0);

        // are we piping?
        if (job.piping_flag) {
            int pipe_fds[2];
            int prev_pipe_read = -1; // Initialize to -1 for the first process

            for (int i = 0; i < job.process_count; i++) {
                if (pipe(pipe_fds) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }

                pid_t pid = fork();

                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                } else if (pid == 0) {
                    // Child process
                    setpgid(0, job.pgid);  // Set the child's PGID to match the job's PGID
                    job.pgid = getpgid(0); // Update the job's PGID to match the new process group

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
                    if (job.background) {
                        setpgid(pid, job.pgid); // Set the child's PGID to match the job's PGID
                    }
                    close(pipe_fds[1]); // Close the write end of the current pipe
                    prev_pipe_read = pipe_fds[0]; // Update the previous pipe's read end
                }
            }

            // Close the last pipe's read end
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            if (job.background) {
                // Add the entire pipeline as a background job
                printf("ADDING: %d\n", job.pgid);
                addToBackgroundJobs(&job);
            } else {
                // Wait for all child processes to finish
                for (int i = 0; i < job.process_count; i++) {
                    wait(NULL);
                }
                // Mark the job ID as unused
                used_job_ids[job.job_id] = 0;
            }

        } else {
            // Check if the user typed "exit" to quit
            if (strcmp(job.processes[0].command, "exit") == 0) {
                freeJob(&job);
                exit(0);
            } else if (strcmp(job.processes[0].command, "cd") == 0) {
                if (job.processes[0].args[1] != NULL) {
                    used_job_ids[job.job_id] = 0;
                    if (chdir(job.processes[0].args[1]) != 0) {
                        perror("cd");
                    }
                } else {
                    fprintf(stderr, "Usage: cd <directory>\n");
                }
            } else if (strcmp(job.processes[0].command, "jobs") == 0) {
                used_job_ids[job.job_id] = 0;
                printBackgroundJobs();
            } else if (strcmp(job.processes[0].command, "fg") == 0) {
                if (job.processes[0].args[1] != NULL) {
                    used_job_ids[job.job_id] = 0;

                } else {
                    fprintf(stderr, "Usage: fg <id>\n");
                }
            } else { 
                // Construct the full path to the executable
                char executable_path[MAX_INPUT_SIZE];
                snprintf(executable_path, sizeof(executable_path), "%s/%s", path, job.processes[0].command);

                pid_t pid = fork();

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
                    if (!job.background) {
                        int status;
                        waitpid(pid, &status, 0);
                        used_job_ids[job.job_id] = 0;
                    } else {
                        // Update the job's PGID to match the new process group
                        job.pgid = pid;
                        // Add the job to the background_jobs array
                        addToBackgroundJobs(&job);
                    }
                }
            }
        }
    }

    // Clean up background jobs
    for (int i = 0; i < background_jobs_size; i++) {
        free(background_jobs[i]);
    }

    free(input_line);
    return 0;
}

