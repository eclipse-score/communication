# inotify Stress Test

Stress test that exercises the `os::InotifyInstanceImpl` inotify wrapper under heavy
concurrent load from multiple processes. Supports three modes, selected via `--mode`.

See also [`inotify_event_loss`](#inotify_event_loss--minimal-standalone-reproducer) below: a
minimal, dependency-free reproducer for a real inotify event-loss/reordering bug that
`--mode burst-loss` revealed on QNX, suitable for sharing externally (e.g. with QNX support).

## What it does

A controller process spawns `M` worker processes. Each worker is optionally assigned a unique
UID/GID. The controller and workers synchronise through `CheckPointControl` objects held in
shared memory, so every cycle is driven in lock-step.

### `--mode watch-churn` (default)

Each worker owns a dedicated sub-directory and file under a shared base folder. On each of the
`N` cycles a worker:

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

### `--mode notify-latency`

Verifies that create/delete notifications on the shared base folder are dispatched to every
watching worker in time, rather than stressing watch add/remove churn.

Each worker adds a single inotify watch on the shared base folder once, up front, and reports
that back to the controller. Once every worker has confirmed its watch is armed, the controller
starts the cycle loop. On each of the `N` cycles:

1. The controller signals every worker (`ProceedToNextCheckpoint`), announcing that it is about
   to create or remove the shared notify test file — this also starts each worker's timeout
   clock.
2. The controller creates the file on even cycles and removes it on odd cycles (workers derive
   the same expectation independently from the cycle index, so no extra communication is
   needed).
3. Each worker waits up to `--notify-timeout-ms` for the matching `IN_CREATE`/`IN_DELETE`
   notification. On success it reports `CheckPointReached`; on timeout, an OS error, or an
   unexpected event it logs the failure and reports `ErrorOccurred`.
4. The controller waits for every worker's checkpoint before moving to the next cycle.

Any worker that misses its expected notification in time fails the whole test.

### `--mode burst-loss`

Verifies that no create/delete notification is lost — or reordered — for a burst of many files
created (then removed) back-to-back in one shot. Models production reports of processes that
watch many inotify events occasionally missing one — suspected to be caused by an inotify event
queue overflow under load.

Each worker adds a single inotify watch on the shared base folder once, up front, reports that
back to the controller, and starts a background thread that continuously drains inotify events
for the entire lifetime of the worker (a burst of `--burst-file-count` files can generate more
events than fit in a single bounded read, so events must be drained continuously, not once per
cycle, to accurately reflect what a real consumer would observe). On each of the `N` cycles:

1. The controller signals every worker (`ProceedToNextCheckpoint`), announcing a new cycle is
   starting. Each worker resets its per-cycle bookkeeping (the full arrival sequence of
   create/delete/`IN_Q_OVERFLOW` events, in the exact order observed) and reports
   `kCycleReadyCheckpoint` before the controller creates any file, to avoid racing the reset
   against the first event.
2. The controller creates, then removes, `--burst-file-count` files (named
   `burst_file_<6-digit index>`) under the shared base folder, back-to-back with no worker
   interaction in between.
3. The controller signals every worker again, announcing the cycle has ended.
4. Each worker waits `--burst-check-delay-ms` for any still in-flight notifications to be
   dispatched and drained, then analyzes its recorded arrival sequence.

Losing notifications under load is **not**, by itself, considered a bug — as long as the kernel's
own `IN_Q_OVERFLOW` indicator correctly signals that its event queue overflowed around that point.
Since the controller creates all files in ascending index order and only then removes them (also
in ascending index order), a healthy arrival sequence — with any `IN_Q_OVERFLOW` markers ignored —
must read `create_0, create_1, ..., create_{N-1}, delete_0, ..., delete_{N-1}`. Each worker walks
its actual arrival sequence once, tracking the next expected create/delete index, and only fails
the cycle (logging details and reporting `ErrorOccurred`) on a genuine bug:

- **Reordering**: an index arrives at or behind the last one already seen for its stream (e.g.
  `create_9` arrives after `create_10`, or a duplicate). This is always a failure, `IN_Q_OVERFLOW`
  or not.
- **Phase violation**: any create arrives after the first delete has been observed. Also always a
  failure regardless of `IN_Q_OVERFLOW`, since the controller never starts removing files until
  every one has been created.
- **Silent (unexplained) loss**: an index arrives *ahead* of the next expected one (a gap) — or,
  for a gap at the very end of the cycle, some trailing indices are never observed at all — and no
  `IN_Q_OVERFLOW` was observed since that stream's last update. This is the "real" bug this test
  hunts for: the kernel dropped a notification without telling us.

Conversely, a gap that *is* immediately preceded by an `IN_Q_OVERFLOW` (mid-sequence or trailing)
is accepted as an explained loss and does not fail the cycle — the worker reports it as an
informational note (via `std::cout`) but still reports `CheckPointReached`.

Any worker that detects a genuine violation in its recorded arrival sequence fails the whole test.

## `inotify_event_loss` — minimal standalone reproducer

The `--mode burst-loss` stress test above **did** reveal a real bug on QNX: the watching
process occasionally misses a create/delete notification, and this loss is **not** indicated by
an `IN_Q_OVERFLOW` event — i.e. the consumer has no way to detect that it happened.

`inotify_event_loss.cpp` is a deliberately minimal, standalone reproducer of exactly this issue,
intended to be handed to QNX support as-is: a single source file with no dependency on anything
but the C/C++ standard library and plain POSIX APIs (`fork()`, `pipe()`, the raw
`<sys/inotify.h>` calls) — no `CheckPointControl`, no `os::InotifyInstanceImpl` wrapper, no
`boost::program_options`, nothing from this repository's own infrastructure.

It only ever uses one watching process and one file-producing process (a single worker was
always sufficient to reproduce the loss), and replaces `CheckPointControl` with a plain pipe:

1. The process (acting as "controller") creates the directory to watch, opens an inotify
   instance, adds a watch (`IN_CREATE | IN_DELETE`), and starts a background thread that
   continuously drains events into a bookkeeping structure: every create/delete/`IN_Q_OVERFLOW`
   event, in the exact order it was observed.
2. It `fork()`s an "event producer" child. The child creates `--file-count` files with
   consecutive, zero-padded names (`evt_file_000000`, `evt_file_000001`, ...) in ascending index
   order, then removes all of them again, also in ascending index order. Once done, it writes a
   single byte to a pipe shared with the parent and exits.
3. The controller blocks on that pipe until the producer signals completion, and reaps it.
4. The controller waits `--check-delay-ms` (default 500) for any still in-flight notifications,
   then analyzes the recorded arrival sequence.

### What counts as a failure

Losing notifications under load is **not**, by itself, considered a bug — as long as the
kernel's own `IN_Q_OVERFLOW` indicator correctly signals that its event queue overflowed around
that point. Since the producer creates all `--file-count` files (ascending index order) before
removing any of them (also ascending index order), a healthy arrival sequence — with any
`IN_Q_OVERFLOW` markers ignored — must read `create_0, create_1, ..., create_{N-1}, delete_0,
delete_1, ..., delete_{N-1}`. The analysis walks the actual arrival sequence once, tracking the
next expected create/delete index, and only fails the cycle (logging details, then terminating
the whole process with a non-zero exit code) on a genuine bug:

- **Reordering**: an index arrives at or behind the last one already seen for its stream (e.g.
  `create_9` arrives after `create_10`, or a duplicate). This is always a failure, `IN_Q_OVERFLOW`
  or not — the kernel re-delivering or misordering an event is never expected/acceptable.
- **Phase violation**: any create arrives after the first delete has been observed. Also always a
  failure regardless of `IN_Q_OVERFLOW`, since the producer never starts removing files until
  every one has been created.
- **Silent (unexplained) loss**: an index arrives *ahead* of the next expected one (a gap) — or,
  for a gap at the very end of the cycle, some trailing indices are never observed at all — and
  no `IN_Q_OVERFLOW` was observed since that stream's last update. This is the "real" bug this
  test hunts for: the kernel dropped a notification without telling us.

Conversely, a gap that *is* immediately preceded by an `IN_Q_OVERFLOW` (mid-sequence or trailing)
is accepted as an explained loss and does not fail the cycle — the test reports it as an
informational note but the cycle still passes.

### `inotify_event_loss` arguments

| Argument              | Default                        | Description                                             |
| --------------------- | ------------------------------- | -------------------------------------------------------- |
| `--cycles`            | 20                              | Number of create/delete cycles to run.                   |
| `--file-count`        | 500                             | Number of files created (then removed) per cycle.        |
| `--check-delay-ms`    | 500                              | Grace period (ms) after a cycle ends before verifying.   |
| `--base-dir`          | `/tmp/inotify_event_loss_test` (`/tmp_discovery/inotify_event_loss_test` on QNX) | Directory to create and watch. |

## Arguments

| Argument                | Default        | Description                                                        |
| ----------------------- | -------------- | ------------------------------------------------------------------ |
| `--num-processes`       | 5              | Number of worker processes to spawn.                               |
| `--cycles`              | 100            | Number of stress cycles before terminating.                        |
| `--base-uid`            | 0              | Worker `N` calls `setuid(base-uid + N)`; `0` skips `setuid`.       |
| `--base-gid`            | 0              | Worker `N` calls `setgid(base-gid + N)`; `0` skips `setgid`.       |
| `--mode`                | `watch-churn`  | `watch-churn`, `notify-latency`, or `burst-loss` (see above).       |
| `--notify-timeout-ms`   | 300            | `notify-latency` only: max time (ms) a worker waits for the expected notification after the controller announces it. |
| `--burst-file-count`    | 500            | `burst-loss` only: number of files created (then removed) per cycle. |
| `--burst-check-delay-ms`| 500            | `burst-loss` only: grace period (ms) a worker waits, after the controller announces a cycle has ended, before checking whether every expected notification arrived. |

When `--base-uid` / `--base-gid` are non-zero the setuid/setgid must succeed, so the binary
has to run as root (e.g. inside the Docker integration-test container). Local non-root runs
should leave these at their default `0`.

## Layout

| File                             | Responsibility                                              |
| -------------------------------- | ----------------------------------------------------------- |
| `inotify_stress_test.cpp`        | Entry point: argument parsing, setup, fork orchestration.   |
| `worker.{h,cpp}`                 | Per-worker cycle loop (all three modes) and UID/GID credential switching. |
| `controller.{h,cpp}`             | Controller loop: signals workers, drives file ops (notify-latency/burst-loss modes), verifies checkpoints. |
| `inotify_stress_test_internal.h` | Shared constants and path helpers.                          |
| `inotify_event_loss.cpp`         | Standalone, dependency-free reproducer (see above) — single file, safe to share externally. |
| `integration_test/`              | pytest wrappers that run the binaries in the test container. |

## Running

Build and run the binary directly:

```sh
bazel run //score/mw/com/test/inotify_stress_test:inotify_stress_test -- --num-processes 10 --cycles 100
```

Run the notify-latency mode:

```sh
bazel run //score/mw/com/test/inotify_stress_test:inotify_stress_test -- --num-processes 10 --cycles 100 --mode notify-latency --notify-timeout-ms 300
```

Run the burst-loss mode:

```sh
bazel run //score/mw/com/test/inotify_stress_test:inotify_stress_test -- --num-processes 10 --cycles 20 --mode burst-loss --burst-file-count 500 --burst-check-delay-ms 500
```

Run the integration test (10 processes, 300 cycles unless noted, distinct UIDs/GIDs from 2000):

```sh
# watch-churn mode
bazel test --config=qnx //score/mw/com/test/inotify_stress_test/integration_test:test_watch_churn

# notify-latency mode
bazel test --config=qnx //score/mw/com/test/inotify_stress_test/integration_test:test_notify_latency

# burst-loss mode (20 cycles, 500 files per cycle)
bazel test --config=qnx //score/mw/com/test/inotify_stress_test/integration_test:test_burst_loss
```

Run `inotify_event_loss` directly:

```sh
bazel run //score/mw/com/test/inotify_stress_test:inotify_event_loss -- --cycles 20 --file-count 500 --check-delay-ms 500
```

Run its integration test:

```sh
bazel test --config=qnx //score/mw/com/test/inotify_stress_test/integration_test:test_event_loss
```

