# Configure Test Application

In order to configure the LoLa test application we need to generate configuration for the test sequences of the
service, and the client app in addition to generating their corresponding `mw_com_config.json` files. This is done
through a python configuration script `config_generator.py` which has to run at build time.
This python script can generate all required configuration files required for the client and service benchmarking apps (see Fig.1 for the schematic representation).
The role of the config_generator is to create consistent pairs of `benchmarking_config.json` and `mw_com_config.json`
for each binary, and guarantee that the keys that are shared between the config files of different binaries have the
same value.
<figure>
<img src="python_config_generator_design.drawio.svg">
<caption>
<b>Fig. 1:</b> Schematic representation of the information flow during configuration.
config_generator.py takes the configuration specified in joined_benchmarking_config.json and the base mw_com_config.json and transforms this
information in all necessery configuration file in order to run the service and it's corresponding n communication
partners.
</caption>
</figure>

For example the `number_of_clients` key is required by the service configuration, to know how many clients
it has to serve, and is required by the client app to know how many client threads to spawn. This number needs to be the
same for both apps otherwise we will get inconsistent behavior.

It is also the job of `config_generator.py` to extract the information on how fast the service writes samples and how fast the client reads,
them from the `joined_benchmark_config.json` and from this information calculate different number of sample slots (`numberOfSampleSlots` key in `mw_com_config.json`) as well as the client specific `max_num_samples` value.
We use the following heuristic:

```
max_num_samples = ceiling(read_cycle_time_ms/send_cycle_time_ms)
numberOfSampleSlots = number_of_clients*max_num_samples
```
i.e. for an example where `read_cycle_time_ms=80` and `send_cycle_time_ms=40` the service writes twice as fast as the
client reads. To guarantee that no data is lost, each client will need two sample slots (`max_num_samples = ceiling(80/40)=ceilint(2)=2`) and if we have twenty such clients, then the service will need in total forty sample slots (`numberOfSampleSlots = 20*2=40`).

Service and Client apps expect two command line arguments, which provide the paths to the two configuration files
```bash
./lola_benchmarking_service path/to/servive_benchmarking_congif.json path/to/service_mw_com_config.json
./lola_benchmarking_client path/to/client_benchmarking_congif.json path/to/client_mw_com_config.json
```

The first configuration file will configure the general behavior of the app while the second file is the
[configuration](../../../../impl/configuration/README.md
) for the `mw::com` runtime.
