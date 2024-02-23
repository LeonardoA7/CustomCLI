#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


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

int SHELL_PID;

int main(int argc, char *argv[]) {

    char *input_line = NULL;
    size_t line_size = 0;
    const char *path = "/bin"; // Initial PATH contains /bin
    int SHELL_PID = getpid();

    while(1) {
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

    }
    
    return 0;
}