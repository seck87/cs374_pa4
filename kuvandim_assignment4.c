// include directives
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// define directives
#define INPUT_LENGTH 2048
#define MAX_ARGS 512
#define MAX_BG_PROCESSES 10

// type definitions

// external variables
int last_fg_status = 0;
int last_fg_signal = 0;
bool last_fg_terminated_by_signal = false;
pid_t bg_processes[MAX_BG_PROCESSES];
int bg_count = 0;

/*
 * The following struct is taken from the file "sample_parser.c" provided in:
 * Operating Systems I (CS 374, Winter 2025), Oregon State University.
 * Course Instructors: Chaudhry, N., Tonsmann, G.
 */
struct command_line {
        char *argv[MAX_ARGS + 1];
        int argc;
        char *input_file;
        char *output_file;
        bool is_bg;
};

// prototypes
struct command_line *parse_input(void);
void execute_input(struct command_line *command);
void check_bg_processes(void);

// main
int main()
{
        struct command_line *curr_command;

        while (true) {
                check_bg_processes();
                curr_command = parse_input();
                execute_input(curr_command);
        }

        return EXIT_SUCCESS;
}

// other functions

/*
 * The following function is taken from the file "sample_parser.c" provided in:
 * Operating Systems I (CS 374, Winter 2025), Oregon State University.
 * Course Instructors: Chaudhry, N., Tonsmann, G.
 */
struct command_line *parse_input(void)
{
        char input[INPUT_LENGTH];
        struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

        // Get input
        printf(": ");
        fflush(stdout);
        fgets(input, INPUT_LENGTH, stdin);

        // Tokenize the input
        char *token = strtok(input, " \n");
        while (token) {
                if(!strcmp(token, "<")) {
                        curr_command->input_file = strdup(strtok(NULL," \n"));
                } else if(!strcmp(token, ">")){
                        curr_command->output_file = strdup(strtok(NULL," \n"));
                } else if(!strcmp(token, "&")){
                        curr_command->is_bg = true;
                } else{
                        curr_command->argv[curr_command->argc++] = strdup(token);
                }
                token=strtok(NULL," \n");
        }

        return curr_command;
}

/*
 * The following function is adapted from coursework provided in:
 * Operating Systems I (CS 374, Winter 2025), Oregon State University.
 * Course Instructors: Chaudhry, N., Tonsmann, G.
 */
void execute_input(struct command_line *command)
{
        if (command->argc == 0) {
                free(command);
                return;
        }

        if (command->argv[0][0] == '#') {
                free(command);
                return;
        }

        if ((strcmp(command->argv[0], "exit")) == 0) {
                // Background processes?
                free(command);
                exit(EXIT_SUCCESS);
        }

        if ((strcmp(command->argv[0], "cd")) == 0) {

                if (command->argc == 1) {
                        char *home = getenv("HOME");
                        chdir(home);
                }

                if (command->argc == 2) {
                        chdir(command->argv[1]);
                }

                free(command);
                return;
        }

        if ((strcmp(command->argv[0], "status")) == 0) {

                if (last_fg_terminated_by_signal) {
                        printf("terminated by signal %d\n", last_fg_signal);
                } else {
                        printf("exit value %d\n", last_fg_status);
                }

                fflush(stdout);
                free(command);
                return;
        }

        // Non built-in commands go here!
        pid_t spawnpid = fork();
        int child_status;

        if (spawnpid == -1) {
                
                free(command);
                exit(EXIT_FAILURE);
                
        } else if (spawnpid == 0) {

                if (command->is_bg) {
                        signal(SIGINT, SIG_IGN);
                } else {
                        signal(SIGINT, SIG_DFL);
                }

                if (command->input_file != NULL) {

                        int fd_in = open(command->input_file, O_RDONLY);

                        if (fd_in == -1) {
                                fprintf(stderr, "cannot open %s for input\n", command->input_file);
                                exit(EXIT_FAILURE);
                        }

                        if (dup2(fd_in, 0) == -1) {
                                exit(EXIT_FAILURE);
                        }

                        close(fd_in);
                }
            
                if (command->output_file != NULL) {

                        int fd_out = open(command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);

                        if (fd_out == -1) {
                                fprintf(stderr, "cannot open %s for output\n", command->output_file);
                                exit(EXIT_FAILURE);
                        }

                        if (dup2(fd_out, 1) == -1) {
                                exit(EXIT_FAILURE);
                        }

                        close(fd_out);
                }

                if (execvp(command->argv[0], command->argv) == -1) {
                        fprintf(stderr, "%s: no such file or directory\n", command->argv[0]);
                        exit(EXIT_FAILURE);
                }

        } else {

                if (command->is_bg) {

                        printf("background pid is %d\n", spawnpid);
                        fflush(stdout);

                        if (bg_count < MAX_BG_PROCESSES) {
                                bg_processes[bg_count++] = spawnpid;
                        }

                } else {

                        waitpid(spawnpid, &child_status, 0);

                        if (WIFEXITED(child_status)) {

                                last_fg_status = WEXITSTATUS(child_status);
                                last_fg_terminated_by_signal = false;

                        } else if (WIFSIGNALED(child_status)) {

                                last_fg_signal = WTERMSIG(child_status);
                                last_fg_terminated_by_signal = true;
                                printf("terminated by signal %d\n", last_fg_signal);
                                fflush(stdout);
                        }
                }
        }

        free(command);
}

/*
 * The following function is adapted from coursework provided in:
 * Operating Systems I (CS 374, Winter 2025), Oregon State University.
 * Course Instructors: Chaudhry, N., Tonsmann, G.
 */
void check_bg_processes(void)
{
        int status;

        for (int i = 0; i < bg_count; ) {

                pid_t result = waitpid(bg_processes[i], &status, WNOHANG);

                if (result > 0) {

                        if (WIFEXITED(status)) {
                                printf("background pid %d is done: exit value %d\n", bg_processes[i], WEXITSTATUS(status));
                        } else if (WIFSIGNALED(status)) {
                                printf("background pid %d is done: terminated by signal %d\n", bg_processes[i], WTERMSIG(status));
                        }

                        fflush(stdout);

                        for (int j = i; j < bg_count - 1; j++) {
                                bg_processes[j] = bg_processes[j+1];
                        }
                        
                        bg_count--;

                } else {

                        i++;
                }

        }
}
