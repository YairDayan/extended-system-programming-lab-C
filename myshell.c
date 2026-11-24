#include <stdio.h>
#include <unistd.h>
#include <linux/limits.h>
#include "LineParser.h"
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

void execute(cmdLine *pCmdLine)
{
  if (pCmdLine->inputRedirect) {
    close(0);
    if (open(pCmdLine->inputRedirect, O_RDONLY) == -1) {
      perror("input redirect");
      _exit(1);
    }
  }
  if (pCmdLine->outputRedirect) {
    close(1);
    if (open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644) == -1) {
      perror("output redirect");
      _exit(1);
    }
  }

  execvp(pCmdLine->arguments[0], (char **)pCmdLine->arguments);
  perror("execvp failed");
  _exit(1);
}

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
    return 0;
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

int main(int argc, char **argv)
{
  char path[PATH_MAX];
  char input[2048];
  cmdLine *parsedLine;
  pid_t pid;
  int debug = 0;
  int i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      debug = 1;
    }
  }
  while (1) {
    if (getcwd(path, PATH_MAX) == NULL) {
      perror("getcwd");
      return 1;
    }

    printf("%s> ", path);
    fflush(stdout);

    if (fgets(input, 2048, stdin) == NULL) {
      break;
    }

    parsedLine = parseCmdLines(input);
    if (parsedLine == NULL) {
      continue;
    }
    if (parsedLine->argCount == 0) {
      freeCmdLines(parsedLine);
      continue;
    }
    if (strcmp(parsedLine->arguments[0], "quit") == 0) {
      freeCmdLines(parsedLine);
      return 0;
    }
    if (strcmp(parsedLine->arguments[0], "cd") == 0) {
      if (parsedLine->argCount < 2) {
        fprintf(stderr, "cd: missing argument\n");
      } else if (chdir(parsedLine->arguments[1]) == -1) {
        perror("cd");
      }
      freeCmdLines(parsedLine);
      continue;
    }
    if (signalCommand(parsedLine)) {
      freeCmdLines(parsedLine);
      continue;
    }
    if (parsedLine->next != NULL) {
      int pipefd[2];
      pid_t pid2;

      if (parsedLine->outputRedirect != NULL ||
          parsedLine->next->inputRedirect != NULL) {
        fprintf(stderr, "pipe: invalid redirection\n");
        freeCmdLines(parsedLine);
        continue;
      }

      if (pipe(pipefd) == -1) {
        perror("pipe");
        freeCmdLines(parsedLine);
        continue;
      }

      pid = fork();
      if (pid == 0) {
        close(1);
        dup(pipefd[1]);
        close(pipefd[1]);
        close(pipefd[0]);
        execute(parsedLine);
      }

      close(pipefd[1]);

      pid2 = fork();
      if (pid2 == 0) {
        close(0);
        dup(pipefd[0]);
        close(pipefd[0]);
        execute(parsedLine->next);
      }

      close(pipefd[0]);

      if (debug) {
        fprintf(stderr, "PID: %d\n", pid);
        fprintf(stderr, "Executing command: %s\n", parsedLine->arguments[0]);
        fprintf(stderr, "PID: %d\n", pid2);
        fprintf(stderr, "Executing command: %s\n",
                parsedLine->next->arguments[0]);
      }

      if (parsedLine->next->blocking) {
        waitpid(pid, NULL, 0);
        waitpid(pid2, NULL, 0);
      }

      freeCmdLines(parsedLine);
      continue;
    }

    pid = fork();
    if (pid == 0) {
      execute(parsedLine);
    }
    if (debug) {
      fprintf(stderr, "PID: %d\n", pid);
      fprintf(stderr, "Executing command: %s\n", parsedLine->arguments[0]);
    }
    if (parsedLine->blocking) {
      waitpid(pid, NULL, 0);
    }
    freeCmdLines(parsedLine);
  }

  return 0;
}
