# Concept - Performance evaluation of LoLa IPC

## Ultimate goals

- **Early Regression Detection**: Continuous evaluation of these benchmarks and comparisons with historical benchmark results, will help detect regressions immediately, as they are introduced.
- **Improvement Planning**: If we collect sufficiently detailed data about the performance of different subsystems,
  we can estimate the maximum performance impact an improvement of any particular subsystem can have.
- **Delivery Evaluation**: We can compare two different implementation ideas, with respect to their performance.

## Considered Approaches

We will employ both **micro-** and **macro-benchmarking** for LoLa. These two approaches serve different roles in performance
evaluation. Micro-benchmarks, measure individual API calls or functions in isolation, similar to unit testing. They're
valuable for pinpointing performance issues and optimizing specific functions during development. However, their
isolated nature can not provide necessary context which is often necessary to accurately reflect real-world
performance bottlenecks.

Macro-benchmarks evaluate overall system performance in realistic scenarios, providing holistic insights. For
applications, this is straightforward: we run the app and record its performance. For libraries like LoLa, however, we
must first design an application that uses the library in a realistic manner and then measure its performance. These
benchmarks are valuable for tracking performance trends and diagnosing systemic issues. While they offer a comprehensive
view of performance, they lack the precision of micro-benchmarks, which are better suited for analyzing and optimizing
specific functions. Together, macro- and micro-benchmarks complement each other, with macro-benchmarks assessing the
broader impact of optimizations and micro-benchmarks zeroing in on individual functions.


| Micro-benchmarks (Individual API Calls)                  | Macro-benchmarks (Configurable Example Application Using LoLa) |
|--------------------------------------------------------------|--------------------------------------------------------------------|
| Using Google Benchmark, can run locally and on the target.   | Using `perf` on the host and `tracelogger` on the target.          |
| Measures execution times of single function calls.           | Runs in a [synthetic environment](broken_link_cf/pages/viewpage.action?spaceKey=psp&title=7-+Platform+Performance+Evaluation). |
| Conceptually analogous to unit tests.                        | Produces aggregate statistics and flamegraphs:                     |
|                                                              | - **Aggregate statistics**: For performance tracking.              |
|                                                              | - **Flamegraphs**: For bottleneck analysis.                        |

### A micro-benchmark suite for LoLa

This means creating a micro-benchmark for all functions that are part of LoLa's public API, using google benchmark.
The process is analogous to creating unit tests. The `api_benchmarking` subfolder will contain all source files for
benchmarking individual api calls, and all testing infrastructure code.
An example of this looks like the following:

```c++
class LolaBenchmarkFixture : public benchmark::Fixture
{
  public:
    LolaBenchmarkFixture()
    {
        this->Repetitions(10);
        this->ReportAggregatesOnly(true);
        this->ThreadRange(1, 1);
        this->UseRealTime();
        this->MeasureProcessCPUTime();
    }
};

BENCHMARK_F(LolaBenchmarkFixture, LoLaInstanceSpecifierCreate)(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto result = score::mw::com::InstanceSpecifier::Create(kBenchmarkInstanceSpecifier);
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK_MAIN();

```

Where the output would look similar to the following
```bash
Run on (8 X 4600 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 1024 KiB (x4)
  L3 Unified 8448 KiB (x1)
Load Average: 0.31, 0.33, 0.52
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
***WARNING*** Library was built as DEBUG. Timings may be affected.
-----------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                                                     Time             CPU   Iterations
-----------------------------------------------------------------------------------------------------------------------------------------------
LolaBenchmarkFixture/LoLaInstanceSpecifierCreate/repeats:10/process_time/real_time/threads:1_mean          1581 ns         1581 ns           10
LolaBenchmarkFixture/LoLaInstanceSpecifierCreate/repeats:10/process_time/real_time/threads:1_median        1560 ns         1560 ns           10
LolaBenchmarkFixture/LoLaInstanceSpecifierCreate/repeats:10/process_time/real_time/threads:1_stddev        33.9 ns         33.9 ns           10

```

**Note:** These micro-benchmark runs are of course hardware dependent and can only be compared with each other if they ran
on the same reference hardware under the same load. However, benchmarks of two different implementations, executed on
the same hardware can reliably be compared.

Only wrinkle that needs to be figured out for this approach is how exactly to collect store and display the benchmark
data, for long term tracking. This issue to be clarified with the performance team.

### Create a configurable macro-benchmark setup for LoLa

We need to create a test application that uses LoLa`s functionality in a way that resembles its real use cases. Ideally
this application should be configurable, and allow us to modify:
- the number of communication partners, i.e. services and clients, that are started,
- the amount of data that is exchanged,
- the frequency with which the data is exchanged and
- the internal behavior of the application.
The latter point is only interesting if there are several ways of achieving the same thing. In this case we would want to
test all of them. For example a client can poll to search for a service, or it can start an asynchronous search and wait
for a callback. Ideally the test application should be configurable at the startup to allow switching between these
behaviors.

<figure>
<img src="lola_macro_benchmark_high_level_design.drawio.svg" style="width:90%">
<figcaption>
<b>High level design</b>: This figure presents a conceptual view of a component of the lola test app. Where one service
is exchanging information with a configurable number of clients via shared memory. Several such components can be
started at the same time.
</figcaption>
</figure>


#### MVP: minimal test app and data pipeline

A basic app using lolas functionality to exchange data between a service provider and a client, with limited
modifiability, can be used to run it in the synthetic environment. In order to set up the full data pipline and collect
data. This minimal application can be seen [here](./macro_benchmark/README.md).
In addition to this a pipeline on the host is also desirable. This requires a bazelized version of `perf`, which
will collect samples from LoLa test app, while it is running, and the bazelized version of [FlameGraph](https://github.com/brendangregg/FlameGraph)
for creating flame graphs from `perf` output.

#### Work beyond The MVP: Improved configurability

The complete LoLa app as represented in the high level design figure is highly configurable, and can run in every
possible mode of operation that is allowed for LoLa apps (for a given service interface).
In particular the following modes of operation need to be configurable:
- Client app can search for the service asynchronously or in polling mode. (I.e. `StartFindService` vs `FindService` in a
  loop)
- Client app can poll for new data or set up an asynchronous handler (i.e. call `GetNewSamples` in a loop or in a callback set by `SetReceiveHandler`).
- The frequency with which the data is written by the service app should be configurable.
- The frequency with which the data is read (if in polling mode) should be configurable.
- Number of clients should be fully configurable. I.e. the appropriate value of some parameters like
  `numberOfMaxSamples` in `mw_com_config.json` is dependent on the number of clients and the data update frequency.
  This invariance should be reflected during the setup.
