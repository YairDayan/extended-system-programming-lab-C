#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "LineParser.h"
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

/* Linked-list node representing a child process managed by the shell */
typedef struct process {
  cmdLine *cmd;         /* the parsed command line*/
  pid_t pid;            /* the process id that is running the
            command*/
  int status;           /* status of the process: RUNNING/SUSPENDED/
           TERMINATED */
  struct process *next; /* next process in chain */
} process;

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

/*
 * Run a command in the current process (typically after fork).
 * Applies input/output redirection if requested, then replaces
 * the process image via execvp. On failure, prints an error and exits.
 */
void execute(cmdLine *pCmdLine)
{
  /* Redirect stdin from a file if "< file" was given */
  if (pCmdLine->inputRedirect) {
    close(0);
    if (open(pCmdLine->inputRedirect, O_RDONLY) == -1) {
      perror("input redirect");
      _exit(1);
    }
  }
  /* Redirect stdout to a file if "> file" was given */
  if (pCmdLine->outputRedirect) {
    close(1);
    if (open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644) == -1) {
      perror("output redirect");
      _exit(1);
    }
  }

  execvp(pCmdLine->arguments[0], (char **)pCmdLine->arguments);
  /* Only reached if execvp failed */
  perror("execvp failed");
  _exit(1);
}

/*
 * Handle built-in signal commands: zzzz (SIGSTOP), kuku (SIGCONT), blast (SIGINT).
 * Expects a process id as the second argument.
 * Returns 1 if the command was a signal command (handled here), 0 otherwise.
 */
int signalCommand(cmdLine *pCmdLine)
{
  int sig;
  int target;
  if (strcmp(pCmdLine->arguments[0], "zzzz") == 0) {
    sig = SIGSTOP;
  } else if (strcmp(pCmdLine->arguments[0], "kuku") == 0) {
    sig = SIGCONT;
  } else if (strcmp(pCmdLine->arguments[0], "blast") == 0) {
    sig = SIGINT;
  } else {
    return 0; /* not a signal command */
  }
  if (pCmdLine->argCount < 2) {
    fprintf(stderr, "%s: missing process id\n", pCmdLine->arguments[0]);
    return 1;
  }
  target = atoi(pCmdLine->arguments[1]);
  if (target <= 0) {
    fprintf(stderr, "%s: invalid process id\n", pCmdLine->arguments[0]);
    return 1;
  }
  if (kill(target, sig) == -1) {
    perror("kill");
  }
  return 1;
}

/* Prepend a new process entry to the process list */
void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
  process *newProcess = malloc(sizeof(process));
  newProcess->cmd = cmd;
  newProcess->pid = pid;
  newProcess->status = RUNNING;
  newProcess->next = *process_list;
  *process_list = newProcess;
}

/* Print all processes currently tracked by the shell */
void printProcessList(process **process_list)
{
  process *curr = *process_list;
  int i;
  const char *statusStr;
  printf("PID\t\tCommand\t\tSTATUS\n");

  while (curr != NULL) {
    if (curr->status == RUNNING) {
      statusStr = "Running";
    } else if (curr->status == SUSPENDED) {
      statusStr = "Suspended";
    } else {
      statusStr = "Terminated";
    }
    printf("%d\t\t", curr->pid);
    for (i = 0; i < curr->cmd->argCount; i++) {
      printf("%s ", curr->cmd->arguments[i]);
    }
    printf("\t\t%s\n", statusStr);

    curr = curr->next;
  }
}

int main(int argc, char **argv)
{
  process *process_list = NULL;
  char path[PATH_MAX];
  char input[2048];
  cmdLine *parsedLine;
  pid_t pid;
  int debug = 0;
  int i;

  /* Enable debug mode if "-d" was passed on the command line */
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      debug = 1;
    }
  }

  /* Main shell loop: print prompt, read, parse, and execute */
  while (1) {
    if (getcwd(path, PATH_MAX) == NULL) {
      perror("getcwd");
      return 1;
    }

    printf("%s> ", path);
    fflush(stdout);

    if (fgets(input, 2048, stdin) == NULL) {
      break; /* EOF (Ctrl+D) */
    }

    parsedLine = parseCmdLines(input);
    if (parsedLine == NULL) {
      continue;
    }
    if (parsedLine->argCount == 0) {
      freeCmdLines(parsedLine);
      continue;
    }

    /* Built-in: quit — exit the shell */
    if (strcmp(parsedLine->arguments[0], "quit") == 0) {
      freeCmdLines(parsedLine);
      return 0;
    }

    /* Built-in: cd — change working directory (must run in the parent) */
    if (strcmp(parsedLine->arguments[0], "cd") == 0) {
      if (parsedLine->argCount < 2) {
        fprintf(stderr, "cd: missing argument\n");
      } else if (chdir(parsedLine->arguments[1]) == -1) {
        perror("cd");
      }
      freeCmdLines(parsedLine);
      continue;
    }

    /* Built-in: procs — list tracked child processes */
    if (strcmp(parsedLine->arguments[0], "procs") == 0) {
      printProcessList(&process_list);
      freeCmdLines(parsedLine);
      continue;
    }

    /* Built-in signal commands (zzzz / kuku / blast) */
    if (signalCommand(parsedLine)) {
      freeCmdLines(parsedLine);
      continue;
    }

    /*
     * Pipeline: cmd1 | cmd2
     * Create a pipe, fork two children, and connect stdout of the first
     * to stdin of the second. Redirection that conflicts with the pipe is rejected.
     */
    if (parsedLine->next != NULL) {
      int pipefd[2];
      pid_t pid2;

      if (parsedLine->outputRedirect != NULL || parsedLine->next->inputRedirect != NULL) {
        fprintf(stderr, "pipe: invalid redirection\n");
        freeCmdLines(parsedLine);
        continue;
      }

      if (pipe(pipefd) == -1) {
        perror("pipe");
        freeCmdLines(parsedLine);
        continue;
      }

      /* First child: write end of the pipe becomes stdout */
      pid = fork();
      if (pid == 0) {
        close(1);
        dup(pipefd[1]);
        close(pipefd[1]);
        close(pipefd[0]);
        execute(parsedLine);
      }

      close(pipefd[1]); /* parent no longer needs the write end */

      /* Second child: read end of the pipe becomes stdin */
      pid2 = fork();
      if (pid2 == 0) {
        close(0);
        dup(pipefd[0]);
        close(pipefd[0]);
        execute(parsedLine->next);
      }

      close(pipefd[0]); /* parent no longer needs the read end */

      if (debug) {
        fprintf(stderr, "PID: %d\n", pid);
        fprintf(stderr, "Executing command: %s\n", parsedLine->arguments[0]);
        fprintf(stderr, "PID: %d\n", pid2);
        fprintf(stderr, "Executing command: %s\n", parsedLine->next->arguments[0]);
      }

      /* Wait for both children unless the command was run in the background */
      if (parsedLine->next->blocking) {
        waitpid(pid, NULL, 0);
        waitpid(pid2, NULL, 0);
      }

      freeCmdLines(parsedLine);
      continue;
    }

    /* Single command (no pipe): fork a child and exec */
    pid = fork();
    if (pid == 0) {
      execute(parsedLine);
    }
    addProcess(&process_list, parsedLine, pid);

    if (debug) {
      fprintf(stderr, "PID: %d\n", pid);
      fprintf(stderr, "Executing command: %s\n", parsedLine->arguments[0]);
    }
    /* Foreground: wait for the child; background (&): do not wait */
    if (parsedLine->blocking) {
      waitpid(pid, NULL, 0);
    }
  }

  return 0;
}
