# inotify Stress Test

Stress test that exercises the `os::InotifyInstanceImpl` inotify wrapper under heavy
concurrent load from multiple processes.

## What it does

A controller process spawns `M` worker processes. Each worker owns a dedicated
sub-directory and file under a shared base folder and is optionally assigned a unique
UID/GID. The controller and workers synchronise through `CheckPointControl` objects held
in shared memory, so every cycle is driven in lock-step.

On each of the `N` cycles a worker:

1. Creates its process directory (skipped when it already exists with the correct
   access rights).
2. Creates its test file (skipped when it already exists with the correct access rights).
3. Adds an inotify watch on the shared base folder
   (`kInCreate | kInDelete`).
4. Removes the watch immediately.
5. Reports the cycle as reached via `CheckPointReached`.

The controller signals each worker to start a cycle (`ProceedToNextCheckpoint`), waits for
every worker to report success or error (`WaitAndVerifyCheckPoint`), removes the per-process
directories so the next cycle re-exercises the creation path, and after all cycles sends
`FinishActions` so workers exit cleanly. Any worker failure fails the whole test.

## Arguments

| Argument          | Default | Description                                                        |
| ----------------- | ------- | ------------------------------------------------------------------ |
| `--num-processes` | 5       | Number of worker processes to spawn.                               |
| `--cycles`        | 100     | Number of stress cycles before terminating.                        |
| `--base-uid`      | 0       | Worker `N` calls `setuid(base-uid + N)`; `0` skips `setuid`.       |
| `--base-gid`      | 0       | Worker `N` calls `setgid(base-gid + N)`; `0` skips `setgid`.       |

When `--base-uid` / `--base-gid` are non-zero the setuid/setgid must succeed, so the binary
has to run as root (e.g. inside the Docker integration-test container). Local non-root runs
should leave these at their default `0`.

## Layout

| File                             | Responsibility                                              |
| -------------------------------- | ----------------------------------------------------------- |
| `inotify_stress_test.cpp`        | Entry point: argument parsing, setup, fork orchestration.   |
| `worker.{h,cpp}`                 | Per-worker cycle loop and UID/GID credential switching.     |
| `controller.{h,cpp}`             | Controller loop: signals workers, verifies checkpoints.     |
| `inotify_stress_test_internal.h` | Shared constants and path helpers.                          |
| `integration_test/`              | pytest wrapper that runs the binary in the test container.  |

## Running

Build and run the binary directly:

```sh
bazel run //score/mw/com/test/inotify_stress_test:inotify_stress_test -- --num-processes 10 --cycles 100
```

Run the integration test (10 processes, 300 cycles, distinct UIDs/GIDs from 2000):

```sh
bazel test //score/mw/com/test/inotify_stress_test/integration_test:test_inotify_stress_test
```
