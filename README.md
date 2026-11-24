# Lab C — Shell: Pipes, Jobs & History

Extended System Programming Laboratory. Extends the Lab 2 shell with pipelines, a process manager, and command history. Includes a standalone pipeline exercise (`mypipeline`).

## What's included

| File | Role |
|------|------|
| `mypipeline.c` | Standalone 2-process pipeline: `ls -lsa \| tail -n 3` |
| `myshell.c` | Shell: Lab 2 features + pipe, `procs`, history (`history` / `!!` / `!n`) |
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

- Everything from Lab 2: prompt, `fork`/`execvp`, `cd`, `quit`, background (`&`), I/O redirection, `zzzz` / `kuku` / `blast`
- Single pipe between two commands, e.g. `ls | wc -l`
- Combined redirection + pipe, e.g. `cat < in.txt | tail -n 2 > out.txt`
- Process manager: `procs` lists forked jobs (`Running` / `Suspended` / `Terminated`)
- History ring of length `HISTLEN` (default 10): `history`, `!!`, `!n`

### Example

```text
/path/to/lab> ./mypipeline
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

- **Part 1** — `mypipeline`: create pipe, fork two children, redirect with `dup`, wait in order  
- **Part 2** — pipe inside `myshell` (one `|`, keep Lab 2 redirection; reject illegal redirect+pipe combos)  
- **Part 3a–3c** — process list (`addProcess` / `printProcessList` / `updateProcessList`), `procs`, signal commands  
- **Part 4** — circular history queue: `history`, `!!`, `!n`

## Submission

Zip with `mypipeline.c`, `myshell.c`, and a makefile that builds `mypipeline` and `myshell`.
