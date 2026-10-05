# A Simulated Operating System in C

An operating-system simulation written from scratch in C: an interactive shell and interpreter, a
multithreaded scheduler, and demand paging backed by an on-disk store.

Built as a team project for an Operating Systems course (COMP 310 / ECSE 427) at McGill
University, Fall 2024.

## What it does

You get a shell prompt. From it you can assign variables, run script files, and launch several
scripts at once under a chosen scheduling policy — including round-robin, where the two scripts
visibly interleave.

Script lines are never all resident. They are faulted in from a backing store into a fixed-size
frame store, with LRU replacement when it fills, so a script can be larger than the memory the
simulation gives you.

## Verified behaviour

Everything below was built and run with **gcc 13.3.0**, not inferred from the source:

- `make` compiles **warning-free** on a clean rebuild.
- `set`, `print`, `echo`, `help`, `quit` and `my_ls` work.
- `run SCRIPT` executes a script file.
- `exec` runs multiple scripts under a policy, and an invalid policy name is **rejected with an
  error** rather than silently defaulting.
- Round robin genuinely interleaves. With one script setting `val1`–`val4` and another `val5`–`val8`:

```
$ ./mysh
exec s1.txt s2.txt RR
val1
val5
val2
val6
val3
val7
val4
val8
```

(`Page fault!` lines are interleaved through that output in practice — that is the paging layer
faulting script lines in, and it is working, not failing.)

## Architecture

| File | Lines | Role |
|---|---|---|
| `interpreter.c` | 1,430 | Command dispatch, the PCB model, the scheduler and its policies, the thread pool, script loading |
| `shellmemory.c` | 270 | Frame store and variable store, LRU bookkeeping, backing-store creation and teardown |
| `shell.c` | 128 | Shell entry, tokenising, prompt loop |
| `memorymanagement.c` | 50 | Frame allocation for loaded script lines |
| headers | 79 | |
| `Makefile` | 12 | Build, with `FRAMESIZE` and `VARMEMSIZE` fixed at compile time |

## The shell

| Command | Behaviour | Verified |
|---|---|---|
| `help` | Lists the commands | ✅ |
| `quit` | Terminates the shell with `Bye!` | ✅ |
| `set VAR STRING` | Assigns a value to shell memory | ✅ |
| `print VAR` | Prints the value assigned to `VAR` | ✅ |
| `echo STRING` | Echoes its argument **literally** — it is not a variable lookup, so `echo greeting` prints `greeting` | ✅ |
| `run SCRIPT.TXT` | Executes a script file | ✅ |
| `exec s1 s2 … POLICY [#] [MT]` | Executes one or more scripts under a scheduling policy | ✅ |
| `my_ls` | Lists the directory via `dirent.h` | ✅ |
| `my_mkdir` `my_touch` `my_cd` | Filesystem commands implemented against the POSIX API rather than shelling out | present |

Note that `help` only documents the five commands inherited from the assignment brief — the shell
implements the rest of the table above, but the help string was never extended to match.

### `exec` — where the project actually lives

```
exec script1.txt script2.txt FCFS
exec script1.txt script2.txt RR30 #
exec script1.txt script2.txt SJF MT
```

| Argument | Meaning |
|---|---|
| `POLICY` | One of `FCFS`, `SJF`, `RR`, `RR30`, `AGING`. Anything else is rejected with an error |
| `#` | Marks the batch as background work |
| `MT` | Hands the processes to the worker thread pool (`workerThreads[2]`) instead of running them to completion on the calling thread |

Both flags are parsed positionally off the end of the argument list, so they compose.

## Scheduling

Each script becomes a PCB. `exec` enqueues one PCB per script and the scheduler drains the queue
under the selected policy; a preempted PCB remembers its position, so a round-robin switch resumes
where it stopped rather than restarting. `playQueue`, `playQueueRR` and `playQueueAging` are the
three dispatch strategies.

| Policy | Behaviour | Status when run |
|---|---|---|
| `FCFS` | First come, first served | runs to completion |
| `RR` | Round robin, one line per turn | runs, and **visibly interleaves** the scripts |
| `SJF` | Shortest job first | runs to completion |
| `RR30` | Round robin with a 30-line quantum | runs to completion |
| `AGING` | Priority scheduling with aging | **does not terminate** — see known issues |

I could not distinguish `SJF` and `RR30` ordering from `FCFS` in a two-script test, so the table
claims only that they run, not that their ordering differs.

## Memory

Both levels are fixed at compile time and deliberately small, so that faulting is observable
rather than theoretical.

- **Frame store** — a `FRAME_STORE_SIZE` array of key/value pairs holding script lines pulled from
  disk. The backing store is a directory of files created at startup and torn down on exit
  (`create_backing_store` / `delete_backing_store`).
- **Variable store** — `VARMEMSIZE` slots for shell variables.

When the frame store is full the least recently used frame is evicted; recency lives in `lru_array`
and a later fault re-reads the line from the backing store. Nothing outside the frame store is ever
assumed to be resident.

## Build and run

Needs `gcc` and `pthreads` — a POSIX environment (Linux, macOS or WSL; not native Windows, since
the thread pool uses `pthread.h`).

```bash
make                        # defaults to framesize=30 varmemsize=30
make framesize=10 varmemsize=10     # smaller stores, for watching eviction
make clean
./mysh
```

The course's Makefile defaulted to `framesize=0 varmemsize=0`, under which `exec` immediately
reports `Ran out of shell memory` — so a fresh clone appeared broken. This repo defaults to 30/30,
which is verified to work. `FRAMESIZE` is measured in pages and `VARMEMSIZE` in variables.

## Example session

```
$ ./mysh
Frame Store Size = 30; Variable Store Size = 30
set greeting hello
print greeting
hello
echo hello
hello
run demo.txt
quit
Bye!
```

## Known issues

Found by running it, and listed rather than hidden:

- **`AGING` does not terminate.** It runs the first few statements correctly, then loops printing
  `Unknown Command` indefinitely. Reproduced twice; the process has to be killed.
- **A large frame store segfaults.** `framesize=200 varmemsize=200` builds but crashes on `exec`.
  The moderate sizes tested (10–30) are fine.
- **Stray `Unknown Command` output** appears in `run` and `exec` output even on successful runs.
- **The backing store directory is not always cleaned up.** `my_backing_store/` survives when the
  process exits abnormally, since teardown only runs on a clean shutdown.

## What was provided, and what we wrote

Being precise, since this was coursework. The course supplied stub files and a Makefile:
`interpreter.c` at **128 lines**, `shell.c` at 65, `shellmemory.c` at 66, and headers totalling 7
— roughly **275 lines**, largely declarations and empty stubs.

Everything that makes it work is ours. `interpreter.c` grew to **1,430** lines, `shellmemory.c` to
270, `shell.c` to 128, and `memorymanagement.c` — absent from the stub entirely — is 50 lines.
That is around **1,880 lines against the original 275**, and every scheduling policy, the thread
pool, the PCB model, the demand paging and the LRU replacement were written by the team.

The assignment scratch material (test scripts, compiled objects, the `mysh` binary and the
handouts) is not included.

## Team

Built as a team project; the work was shared evenly between two people, and everything above is the
combined result.

## Licence

MIT — see [LICENSE](LICENSE).
