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

/* Circular history queue of unparsed command lines (Part 4) */
#define HISTLEN 10
typedef struct history {
  char *lines[HISTLEN]; /* heap copies of previous command lines */
  int oldest;           /* index of the oldest entry */
  int next;             /* index where the next entry will be written */
  int count;            /* how many entries are currently stored */
} history;

/* Process status values stored in process.status */
#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

/* Forward declarations */
void execute(cmdLine *pCmdLine);
int signalCommand(cmdLine *pCmdLine, process **process_list);
void addProcess(process **process_list, cmdLine *cmd, pid_t pid);
void printProcessList(process **process_list);
void updateProcessStatus(process *process_list, int pid, int status);
void updateProcessList(process **process_list);
void freeProcessList(process *process_list);
void addHistory(history *hist, const char *line);
char *getLastCommand(history *hist);
char *getHistory(history *hist, int n);
void printHistory(history *hist);
void freeHistory(history *hist);

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
 * Expects a process id as the second argument. On success, updates that process
 * in process_list to SUSPENDED / RUNNING / TERMINATED as appropriate.
 * Returns 1 if the command was a signal command (handled here), 0 otherwise.
 */
int signalCommand(cmdLine *pCmdLine, process **process_list)
{
  int sig;
  int target;
  int newStatus;
  if (strcmp(pCmdLine->arguments[0], "zzzz") == 0) {
    sig = SIGSTOP;
    newStatus = SUSPENDED;
  } else if (strcmp(pCmdLine->arguments[0], "kuku") == 0) {
    sig = SIGCONT;
    newStatus = RUNNING;
  } else if (strcmp(pCmdLine->arguments[0], "blast") == 0) {
    sig = SIGINT;
    newStatus = TERMINATED;
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
  } else {
    /* kill succeeded — reflect the new status in the process list */
    printf("%s: signal sent to process %d\n", pCmdLine->arguments[0], target);
    updateProcessStatus(*process_list, target, newStatus);
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

/*
 * Refresh statuses, print the process table, then drop entries that were
 * shown as Terminated so they appear once and are removed afterwards.
 */
void printProcessList(process **process_list)
{
  updateProcessList(process_list); /* refresh RUNNING / SUSPENDED / TERMINATED */
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

  /* Remove freshly terminated processes after printing them once */
  process *prev = NULL;
  curr = *process_list;
  while (curr != NULL) {
    if (curr->status == TERMINATED) {
      process *toDelete = curr;
      curr = curr->next;
      if (prev == NULL) {
        *process_list = curr;
      } else {
        prev->next = curr;
      }
      freeCmdLines(toDelete->cmd);
      free(toDelete);
    } else {
      prev = curr;
      curr = curr->next;
    }
  }
}

/* Find the process with the given pid and set its status */
void updateProcessStatus(process *process_list, int pid, int status)
{
  process *curr = process_list;
  while (curr != NULL) {
    if (curr->pid == pid) {
      curr->status = status;
      break;
    }
    curr = curr->next;
  }
}

/*
 * Non-blocking waitpid on every tracked child to refresh its status.
 * WNOHANG: do not block; WUNTRACED / WCONTINUED: notice stop and resume.
 */
void updateProcessList(process **process_list)
{
  process *curr = *process_list;
  int status;
  int result;

  while (curr != NULL) {
    result = waitpid(curr->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if (result == -1) {
      /* No such child anymore (already reaped or gone) */
      curr->status = TERMINATED;
    } else if (result == curr->pid) {
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        curr->status = TERMINATED;
      } else if (WIFSTOPPED(status)) {
        curr->status = SUSPENDED;
      } else if (WIFCONTINUED(status)) {
        curr->status = RUNNING;
      }
    }
    curr = curr->next;
  }
}

/* Free every process node and the cmdLine it owns */
void freeProcessList(process *process_list)
{
  process *curr = process_list;
  while (curr != NULL) {
    process *next = curr->next;
    freeCmdLines(curr->cmd);
    free(curr);
    curr = next;
  }
}

/*
 * Append a copy of line to the circular history queue.
 * When full, drop the oldest entry first (O(1) via oldest / next indices).
 */
void addHistory(history *hist, const char *line)
{
  char *copy = malloc(strlen(line) + 1);
  strcpy(copy, line);

  if (hist->count == HISTLEN) {
    free(hist->lines[hist->oldest]);
    hist->oldest = (hist->oldest + 1) % HISTLEN;
    hist->count--;
  }

  hist->lines[hist->next] = copy;
  hist->next = (hist->next + 1) % HISTLEN;
  hist->count++;
}

/*
 * Return the most recent command that is not the built-in "history"
 * (used by !!). Returns NULL if none found.
 */
char *getLastCommand(history *hist)
{
  int i;
  int index;

  for (i = 0; i < hist->count; i++) {
    index = (hist->next - 1 - i + HISTLEN) % HISTLEN;
    if (strcmp(hist->lines[index], "history") != 0) {
      return hist->lines[index];
    }
  }
  return NULL;
}

/* Return history entry n (1-based), or NULL if n is out of range */
char *getHistory(history *hist, int n)
{
  if (n < 1 || n > hist->count) {
    return NULL;
  }
  return hist->lines[(hist->oldest + n - 1) % HISTLEN];
}

/* Print all history entries with their 1-based index */
void printHistory(history *hist)
{
  int i;
  int index;

  for (i = 0; i < hist->count; i++) {
    index = (hist->oldest + i) % HISTLEN;
    printf("%d %s\n", i + 1, hist->lines[index]);
  }
}

/* Free all stored history strings and reset the queue */
void freeHistory(history *hist)
{
  int i;
  int index;

  for (i = 0; i < hist->count; i++) {
    index = (hist->oldest + i) % HISTLEN;
    free(hist->lines[index]);
    hist->lines[index] = NULL;
  }
  hist->oldest = 0;
  hist->next = 0;
  hist->count = 0;
}

int main(int argc, char **argv)
{
  process *process_list = NULL; /* head of the shell's process list */
  char path[PATH_MAX];
  char input[2048];
  cmdLine *parsedLine;
  pid_t pid;
  int debug = 0;
  int i;
  history hist = {0}; /* circular command-line history (Part 4) */

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

    /* Strip trailing newline; skip empty lines */
    input[strcspn(input, "\n")] = '\0';
    if (input[0] == '\0') {
      continue;
    }

    /* History recall: !! (last non-history cmd) or !n (entry n) */
    if (input[0] == '!') {
      char *cmd;

      if (strcmp(input, "!!") == 0) {
        cmd = getLastCommand(&hist);
      } else {
        cmd = getHistory(&hist, atoi(input + 1));
      }

      if (cmd == NULL) {
        printf("No such command in history\n");
        continue;
      }

      printf("%s\n", cmd); /* echo the recalled command */
      strcpy(input, cmd);  /* run it as if the user typed it */
    }

    addHistory(&hist, input); /* store the unparsed line */
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
      freeProcessList(process_list); /* release tracked processes before exit */
      freeHistory(&hist);            /* release history strings before exit */
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

    /* Built-in: history — print the circular command history */
    if (strcmp(parsedLine->arguments[0], "history") == 0) {
      printHistory(&hist);
      freeCmdLines(parsedLine);
      continue;
    }

    /* Built-in signal commands (zzzz / kuku / blast) */
    if (signalCommand(parsedLine, &process_list)) {
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

      cmdLine *rightCmd = parsedLine->next;

      if (debug) {
        fprintf(stderr, "PID: %d\n", pid);
        fprintf(stderr, "Executing command: %s\n", parsedLine->arguments[0]);
        fprintf(stderr, "PID: %d\n", pid2);
        fprintf(stderr, "Executing command: %s\n", rightCmd->arguments[0]);
      }

      /* Wait for both children unless the command was run in the background */
      if (rightCmd->blocking) {
        waitpid(pid, NULL, 0);
        waitpid(pid2, NULL, 0);
      }

      parsedLine->next = NULL;
      addProcess(&process_list, parsedLine, pid);
      addProcess(&process_list, rightCmd, pid2);

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
  freeProcessList(process_list); /* EOF path: free any remaining process nodes */
  freeHistory(&hist);            /* EOF path: free history strings */
  return 0;
}
