# Lab C — Shell: Pipes, Jobs & History

Extended System Programming Laboratory. Extends the Lab 2 shell with pipelines, a process manager, and command history. Includes a standalone pipeline exercise (`mypipeline`).

## What's included

| File | Role |
|------|------|
| `mypipeline.c` | Standalone 2-process pipeline: `ls -lsa \| tail -n 3` |
| `myshell.c` | Shell with pipes, process list, signals, and history |
| `looper.c` | Test process that prints and forwards `SIGINT`, `SIGTSTP`, `SIGCONT` |
| `LineParser.c` / `LineParser.h` | Course-provided command-line parser (supports `|`) |
| `Makefile` | Builds `mypipeline`, `myshell`, and `looper` |

## Build

Requires Linux (`linux/limits.h`). On macOS, build inside a Linux container or lab machine.

```bash
make              # builds mypipeline, myshell, looper
make mypipeline
make myshell
make clean
```

## Run

```bash
./mypipeline
./myshell
./myshell -d      # debug: print PID and command to stderr
./looper &
```

### Shell features

- Lab 2 base: prompt with cwd, `fork`/`execvp`, `cd`, `quit`, background (`&`), I/O redirection
- One pipe between two commands via `executePipe`, e.g. `ls | wc -l`
- Combined redirection + pipe, e.g. `cat < in.txt | tail -n 2 > out.txt`
- Illegal redirect+pipe combos are rejected (no new processes)
- Process manager: `procs` refreshes statuses then prints `Running` / `Suspended` / `Terminated` (terminated entries are removed after one print)
- Signals: `zzzz` / `kuku` / `blast` update the process list on success
- History ring of length `HISTLEN` (default 10): `history`, `!!`, `!n`

### Example

```text
/path/to/lab> ./myshell
/path/to/lab> ls -lsa | tail -n 3
/path/to/lab> sleep 5 &
/path/to/lab> procs
/path/to/lab> ./looper &
/path/to/lab> zzzz 12345
/path/to/lab> kuku 12345
/path/to/lab> blast 12345
/path/to/lab> history
/path/to/lab> !!
/path/to/lab> !2
/path/to/lab> quit
```

## Tasks covered

| Part | What |
|------|------|
| 1 | `mypipeline` — pipe, two children, `dup`, wait in order |
| 2 | Pipe inside `myshell` (`executePipe`) |
| 3a–3c | Process list, `procs`, status updates, signal commands |
| 4 | Circular history: `history`, `!!`, `!n` |

## Submission

Zip with `mypipeline.c`, `myshell.c`, and a makefile that builds `mypipeline` and `myshell`. Use `#include <linux/limits.h>` (no macOS fallback) for the submitted sources.
