#include "parser/ast.h"
#include "shell.h"
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h> //receive and handle signals
#include <fcntl.h> //open files

#define STDIN 0
#define STDOUT 1
#define PIPE_RD 0
#define PIPE_WR 1

void sigintHandler(int signum) {
    printf("Signal received: %d \n", signum);
}

void sigstopHandler(int signum) {
    printf("Signal received: %d \n", signum);
}

void initialize(void) {
    if (prompt)  /* This code will be called once at startup */
        prompt = "$ "; //v u s h 
}

void exec_command(node_t *node) {
    pid_t pid;
    char *program = node->command.program;
    char **argv = node->command.argv;

    if (strcmp(program, "exit") == 0) exit(atoi(argv[1])); //argv is a string that needs to be converted to int
    else if (strcmp(program, "cd") == 0) { //cd built in
        if (chdir(argv[1]) == -1) fprintf(stderr, "No such file or directory"); //change directory to the entered path
    }
    else if (strcmp(program, "set") == 0) putenv(node->command.argv[1]); //set environmental variable
    else if (strcmp(program, "unset") == 0) unsetenv(node->command.argv[1]); //unset environmental variable
    else {
        pid = fork(); //create child process
        if (pid < 0) { //fork failed
            fprintf(stderr, "Fork failed \n");
            exit(1);
        }
        else if (pid == 0) { //child (new process)
            if (execvp(program, argv) < 0) perror(""); //check if execvp executed succesfully
        }
        else waitpid(-1, NULL, 0); //parent, wait for the child process to be finished executing
    }
}

void exec_pipe(node_t *node) {
    int n_parts = (int) node->pipe.n_parts;
    node_t **parts = node->pipe.parts;
    pid_t pid;

    for (int i = 0; i < n_parts; i++) {
        int fd[2];
        pipe(fd);

        if (i >= n_parts - 1) run_command(parts[i]); //reach the end of the parts
        else {
            pid = fork();
            if (pid < 0) fprintf(stderr, "Forking failed \n");
            else if (pid == 0) {
                close(fd[PIPE_RD]); //close read side of pipe
                dup2(fd[PIPE_WR], STDOUT); //close STDOUT, which normally gets written to. Output of child process will be written into the pipe
                run_command(parts[i]); //run the command
                exit(0);
            }
            else {
                close(fd[PIPE_WR]); //close writing side of the pipe
                dup2(fd[PIPE_RD], STDIN); //close STDIN, duplicate read file descriptor. Input of parent process will be written from the pipe
            }
        }
    }
}

void exec_sequence(node_t *node1, node_t *node2) { //Simply run two commands after each other
    run_command(node1);
    run_command(node2);
}

void exec_redirect(node_t *node) { //Determine which redirect needs to be executed
    int fd;
    pid_t pid;

    pid = fork();
    if (pid < 0) printf("Fork failed \n");
    else if (pid == 0) {
        switch(node->redirect.mode) {
            case REDIRECT_APPEND:
                fd = open(node->redirect.target, O_APPEND | O_WRONLY); //can write to the file but only append
                break;
            case REDIRECT_DUP:
                fd = node->redirect.fd2;
                break;
            case REDIRECT_INPUT:
                fd = open(node->redirect.target, O_RDONLY);
                break;
            case REDIRECT_OUTPUT:
                fd = open(node->redirect.target, O_CREAT | O_WRONLY | O_TRUNC); //Write to a newly created file (can be overwritten)
                break;
            default:
                fprintf(stderr, "Redirect command not recognized \n");
        }
        if (fd < 0) exit(1);
        dup2(fd, node->redirect.fd);
        run_command(node->redirect.child);
        exit(0); //Necessary to exit if statement (avoid duplicate print)
      }
    else waitpid(pid, NULL, 0);
}

void exec_subshell(node_t *node) {
    pid_t pid;
    pid = fork();
    if (pid < 0) fprintf(stderr, "Fork failed \n");
    else if (pid == 0) {
        run_command(node->subshell.child);
        exit(0);
    }
    else waitpid(-1, NULL, 0);
}

void run_command(node_t *node) {
    signal(SIGINT, sigintHandler); //Handle CTRL+C
    switch(node->type) {
        case NODE_COMMAND:
            exec_command(node);
            break;
        case NODE_PIPE:
            exec_pipe(node);
            break;
        case NODE_SEQUENCE:
            exec_sequence(node->sequence.first, node->sequence.second);
            break;
        case NODE_REDIRECT:
            exec_redirect(node);
            break;
        case NODE_SUBSHELL:
            exec_subshell(node);
        default:
            break;
        }
    if (prompt)
        prompt = "$ ";
}
