# Generic LoLa app for performance benchmarking

The LoLa app contained in this folder, represents a straightforward use case for LoLa where `n` clients are
communicating with one service. A service is created first and updates and sends data until all clients come online,
read as much data as they want and then unsubscribe. When all clients have signalled that they have unsubscribed the
service shuts down and the test is over.

## Test sequence description

The test sequence is very straightforward for both the service and the client.
When the service comes online it starts sending data comprised of large array of `1`s. The service does not stop until
it either receives an interrupt signal or a preconfigured number of clients, signal they are done. After which the
server will shut down. <br>
Similarly, the client after coming online will create a predetermined number of threads, one for each receiver. Each
receiver will search for the service, in a loop until they find one or get interrupted. Each receiver will then ask
for the data, until they receive a predetermined amount of new samples. Finally, they will signal that they are done and
shut down.

Table 1 describes the sequence of actions taken in bullet points. And the sequence diagram shows a more comprehensive
view.

<table>
    <caption style="caption-side:bottom" style="text-align:left"> <b>Table 1:</b> Sequence of important actions perfomed by the service and the client aplications.
    </caption>
<td>

| Service                                     |
|-------------------------------------------- |
| runtime::InitializeRuntime(...)             |
| InstanceSpecifier::Create(...)              |
| skeleton::Create(...)                       |
| skeleton::OfferService(...)                 |
| **loop:** until all proxies are unsubscribed <br> - skeleton.event_name.Allocate() <br> - update event <br> - skeleton.event_name.Send(...) <br>  - check if all proxies are unsubscribed    |
| skeleton::StopOfferService(...)             |

</td>
<td>

|     Client                     |
|--------------------------------|
| runtime::InitializeRuntime(...)|
| InstanceSpecifier::Create(...) |
| **loop:** until the service is found <br> - proxy::FindService(...) |
| proxy::Create(...) |
| proxy.event_name.Subscribe() |
| **loop:** until the predetermined number of samples is received<br>-lola_proxy.event_name.GetNumNewSamplesAvailable()<br>-proxy.event_name.GetNewSamples(...) |
| proxy.even_name.Unsubscribe()|
 | signal the skeleton that this proxy is done |
</td>

</table>

<figure>
<img src="BenchmarkTestDiagram.drawio.svg" alt="Sequence diagram of the benchmark." style="width:100%">
<figcaption>Fig.1: Sequence diagram, representing the entire benchmark.
</figcaption>
</figure>

## How to run

This test was created to be run using the synthetic test environment. There are several ways to run the test manually
as detailed later but in order to run the test in the synthetic environment the following
[documentation](../../../../../test/pas/perf_tests/tools/artificial_load/launcher/README.md) of the environment and the
documentation of the [test](../../../../../test/pas/perf_tests/tools/artificial_load/profiles/in_vehicle/launcher_conf.json)
should be consulted. An example configuration can be seen [here](../../../../../test/pas/perf_tests/tools/artificial_load/profiles/in_vehicle/launcher_conf.json).

### Local runs from the command line

Since they do not require any external orchestration they can easily be launched from the command line.

Thus, after the `lola_benchmarking_service` and `lola_benchmarking_client` apps have been compiled,
```bash
bazel build --config=spp_host_clang //score/mw/com/test/benchmark:lola_benchmarking_service
bazel build --config=spp_host_clang //score/mw/com/test/benchmark:lola_benchmarking_client
```
they can be started from the command line.

```bash
lola_benchmarking_service path/to/service_config.json path/to/mw_com_config.json
lola_benchmarking_client path/to/client_config.json path/to/mw_com_config.json
```
For the logging to work you need to copy the `logging.json` into the `etc/` of the destination where the
`lola_benchmarking_service` and `lola_benchmarking_client` apps are run from, or export the `MW_LOG_CONFIG_FILE`
environment variable with the location of the `logger.json` file.

**Note:** If the Service crashes it might not have the opportunity to clean up its shared memory files. This might lead
to problems when running the client again.

# Using the `perf_run` target

This is currently the main intended way to run this test on the host. Which uses a python target to compile and run the
binaries. Both `lola_benchmarking_service` and `lola_benchmarking_client` instances run with a `perf` instance attached
which will sample the application intermittently, which measures, the distribution of average time spent in any given
sub-process.

The following command executes the target. Note that setting the compilation mode to debug and setting further cxx
options are not necessary but improve the amount of information `perf` can collect.
```bash
bazel run --config=spp_host_clang //score/mw/com/test/benchmark:perf_run --test_output=streamed --nocache_test_results --compilation_mode=dbg --cxxopt=-Og --cxxopt=-g
```

#### Prepare the `perf` run
`perf` requires a high level of access, since it has to monitor the communication between the CPU and the running
binary. These commands need to be run (once after a restart) before perf can collect data.
```
sudo sh -c 'echo 1 >/proc/sys/kernel/perf_event_paranoid'
sudo sh -c 'echo 0 >/proc/sys/kernel/kptr_restrict'
```
#### Extracting Analyzing `perf` data
 **ToDo**: automate the saving of `data.perf` in an appropriate destination.

 In order to visualize the data collected by `perf`, this [flame graph tool](https://github.com/brendangregg/FlameGraph) can be used.


## Expected command line arguments

### Command Line Arguments for `service`

Both service and client apps take two positional arguments the first is the path to the app configuration and the second
is the path to the `mw::com` service instance manifest (i.e. `mw_com_config.json`). Examples of these files can be found in
the config/ subfolder, together with their respective schemas, which document and explain their structure. **Note**:
since `mw_com_config.json` is more general than this benchmark, its schema is located in
`mw/com/impl/configuration/ara_com_config_schema.json`.
