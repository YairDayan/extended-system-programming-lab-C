#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <string.h>

/*
 * Custom signal handler: print the received signal name, then
 * re-raise it with the default action so the process actually stops/continues/exits.
 * For SIGTSTP/SIGCONT, also re-arm the opposite handler so toggling keeps working.
 */
void handler(int sig)
{
  printf("\nRecieved Signal : %s\n", strsignal(sig));
  if (sig == SIGTSTP) {
    /* After stop, listen again for continue */
    signal(SIGCONT, handler);
  } else if (sig == SIGCONT) {
    /* After continue, listen again for stop */
    signal(SIGTSTP, handler);
  }
  /* Restore default disposition, then re-raise so the kernel acts on it */
  signal(sig, SIG_DFL);
  raise(sig);
}

/*
 * Endless loop used to test signal delivery (SIGINT, SIGTSTP, SIGCONT)
 * from another process or from the shell's built-in signal commands.
 */
int main(int argc, char **argv)
{

  printf("Starting the program\n");
  signal(SIGINT, handler);
  signal(SIGTSTP, handler);
  signal(SIGCONT, handler);

  while (1) {
    sleep(1);
  }

  return 0;
}
