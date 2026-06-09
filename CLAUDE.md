# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
make              # build myshell binary (gcc -Wall -Wextra -g)
make run          # build and launch the shell
make test         # build, then run `echo "exit" | ./myshell` smoke test
make clean        # remove binary
```

There is no configure step, no external dependencies beyond libc and POSIX APIs. All `.c` files are compiled together in a single `gcc` invocation.

To run the pipe integration test suite manually:
```bash
chmod +x test_pipe.sh && ./test_pipe.sh
```

## Architecture

MyShell is a custom Unix shell implementing a REPL loop with pipes, redirection, wildcards, aliases, builtin commands, and persistent history.

### Two-Branch Execution Model

The core architectural decision lives in `myshell.c` main loop: after parsing input into `args[]`, it scans for `|`. This splits into two entirely separate code paths:

- **Single-command path** (`has_pipe == 0`): Builtins run directly in the parent process (no fork) via `handle_builtin()`, which internally applies redirect with `is_child=0` (save/restore semantics). External commands are forked.
- **Pipeline path** (`has_pipe == 1`): Everything runs in child processes. `|` tokens are NULLed to segment `args[]` into sub-arrays. N-1 pipes are created for N commands. Each child `dup2`s the appropriate pipe ends, closes all pipe fds, then checks for builtin or external. **Critical**: builtins in the pipeline path must `exit()` the child process after running.

### Module Map

| File | Responsibility |
|---|---|
| `myshell.c` | REPL loop, input parsing, fork/exec, pipe orchestration, signal setup |
| `builtins.c` / `.h` | Builtin command dispatch (`cd`, `exit`, `history`, `export`, `alias`) + history persistence to `~/.myshell_history` |
| `redirect.c` / `.h` | `apply_redirect()` scans `args` for `>`, `>>`, `<`, opens files, dup2s fds. `restore_redirect()` reverts stdout/stdin for parent-process builtins. The `is_child` flag controls whether to save fds before redirecting. |
| `alias.c` / `.h` | Static alias table (max 128). `builtin_alias()` for `alias name=value`. `expand_alias()` does a simple `args[0]` string swap. |
| `wildcard.c` / `.h` | `expand_wildcards()` uses `glob(3)` with `GLOB_NOCHECK` to expand `*`/`?` patterns, builds a new args array, copies back over the original. |

### Execution Order in the REPL Loop

1. Print colored prompt (green arrow on success, red on failure)
2. Read input, strip newline, add to history
3. `strtok` split into `args[]`
4. `expand_alias(args)` — may replace `args[0]`
5. `expand_wildcards(args)` — may expand `*`/`?` into multiple args
6. Scan for `|` → branch into single or pipeline path
7. Update `last_status` from child exit code for next prompt color

### Redirect Internals

`apply_redirect(args, ctx, is_child)` scans the args array for redirection operators. When found, it NULLs the operator position to truncate args, opens the target file, and `dup2`s over the relevant fd. For parent-process builtins (`is_child=0`), it saves the original fd via `dup()` so `restore_redirect()` can put it back. Only one redirection operator is handled per command (first match wins in scan order: `>>` > `>` > `<`).

### History

Commands are persisted to `~/.myshell_history`. Loading happens once at startup via `load_history_from_file()`. Each input line is appended immediately (write-through) via `add_to_history()`. The in-memory ring buffer holds up to `MAX_HISTORY` (100) entries, evicting oldest when full.
