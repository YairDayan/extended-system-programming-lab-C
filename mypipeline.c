#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(void)
{
  char *cmd1[] = {"ls", "-lsa", NULL};
  char *cmd2[] = {"tail", "-n", "3", NULL};

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    return 1;
  }

  fprintf(stderr, "(parent_process>forking…)\n");
  pid_t pid = fork();
  if (pid == 0) {
    fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe…)\n");
    close(1);
    dup(pipefd[1]);
    close(pipefd[1]);
    fprintf(stderr, "(child1>going to execute cmd: ls -lsa)\n");
    execvp(cmd1[0], cmd1);
    perror("execvp");
    _exit(1);
  } else if (pid != 0) {
    fprintf(stderr, "(parent_process>created process with id: %d)\n", pid);
    fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n");
    close(pipefd[1]);

    fprintf(stderr, "(parent_process>forking…)\n");
    pid_t pid2 = fork();
    if (pid2 == 0) {
      fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe…)\n");
      close(0);
      dup(pipefd[0]);
      close(pipefd[0]);
      fprintf(stderr, "(child2>going to execute cmd: tail -n 3)\n");
      execvp(cmd2[0], cmd2);
      perror("execvp");
      _exit(1);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", pid2);
    fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n");
    close(pipefd[0]);
    fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n");
    waitpid(pid, NULL, 0);
    waitpid(pid2, NULL, 0);
    fprintf(stderr, "(parent_process>exiting…)\n");
  }

  return 0;
}
