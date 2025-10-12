# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************
import os
import subprocess
import sys
import signal
from typing import List, Optional
import json
import shutil


def clean_up(mw_com_config_path: str,
             processes: Optional[List[subprocess.Popen]]=None):

    with open(mw_com_config_path, "r") as mw_com_config_fp:
        mw_com_config = json.load(mw_com_config_fp)
        # NOTE: this is not a general way to parse the mw_com_config and will
        # stop working if the config is changed
        service_id = mw_com_config["serviceTypes"][0]["bindings"][0]["serviceId"]
        instance_id = mw_com_config["serviceInstances"][0]["instances"][0]["instanceId"]
        tag = f"{service_id:016}-{instance_id:05}"
        cleanup_list = [f"lola-ctl-{tag}", f"lola-ctl-{tag}-b", f"lola-data-{tag}"]

        for shm_file in cleanup_list:
            shm_path = f"/dev/shm/{shm_file}"
            if os.path.isfile(shm_path):
                os.remove(shm_path)

    service_discovery_path = f"/tmp/mw_com_lola/service_discovery/{service_id}"
    if os.path.isdir(service_discovery_path):
        shutil.rmtree(service_discovery_path)
    if processes:
        for p in processes:
            p.terminate()
    else:
        print("was none")


class CommandLineArguments:

    def __init__(self):
        # Parse command-line arguments
        if len(sys.argv) != 3:
            print("Usage: python script.py <LOGGING_JSON_PATH>",
                  "<path/to/config_dir> ")
            print("config_dir will usually be an output artifact of a bazel.")
            sys.exit(1)

        run_dir = "/".join(sys.argv[0].split("/")[0:-1])
        config_dir = sys.argv[2]

        self.client_mw_com_config_path = f"{run_dir}/{config_dir}/client_mw_com_config.json"
        self.service_mw_com_config_path = f"{run_dir}/{config_dir}/service_mw_com_config.json"

        self.client_config_path = f"{run_dir}/{config_dir}/client_benchmark_config.json"
        self.service_config_path = f"{run_dir}/{config_dir}/service_benchmark_config.json"

        self.service_path = f"{run_dir}/lola_benchmarking_service"
        self.client_path = f"{run_dir}/lola_benchmarking_client"


def launch_processes(cla: CommandLineArguments()):

    service_proc = subprocess.Popen([cla.service_path,
                                     cla.service_config_path,
                                     cla.client_mw_com_config_path])
    service_pid = service_proc.pid

    service_perf = subprocess.Popen(
            ["perf", "record", "-F", "99", "-a", "-g", "-p",
             str(service_pid), "-o", "data_service.perf"])

    client_proc = subprocess.Popen([cla.client_path,
                                    cla.client_config_path,
                                    cla.service_mw_com_config_path])
    client_pid = client_proc.pid

    client_perf = subprocess.Popen(
            ["perf", "record", "-F", "99", "-a", "-g", "-p",
             str(client_pid), "-o", "data_client.perf"])

    service_proc.wait()
    client_proc.wait()

    clean_up(cla.client_mw_com_config_path, [service_proc, client_proc,
                                             service_perf, client_perf])
    print("All Done")


def main():
    cla = CommandLineArguments()

    def signal_handler(sig, frame):
        print("\nSIGINT received!")
        clean_up(cla.service_mw_com_config_path)
        sys.exit(0)
    signal.signal(signal.SIGINT, signal_handler)

    print("Running. Press Ctrl+C to exit early.")
    launch_processes(cla)


if __name__ == "__main__":
    main()
