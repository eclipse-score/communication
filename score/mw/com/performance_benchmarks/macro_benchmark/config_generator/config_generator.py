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
import sys
import json
import os
import math

from typing import Dict, List, Tuple, Union, Any


def load_json(json_path: str) -> Dict:
    with open(json_path, 'r') as json_file:
        loaded_json = json.load(json_file)
    return loaded_json


def save_json(json_path: str, json_data: Dict):
    os.makedirs(os.path.dirname(json_path), exist_ok=True)
    with open(json_path, "w") as json_file:
        json.dump(json_data, json_file, indent=4, sort_keys=True)


def calculate_slot_numbers(joined_config_json: dict):
    '''
    Return the numberOfSampleSlots value needed for the test event, so that no event samples get lost as well as the
    maxSamplesCount, with which client apps should subscribe. The values are calculated based on the properties:
    send_cycle_time_ms, read_cycle_time_ms, number_of_clients from the joined_config_json

            Parameters:
                    joined_config_json (dict): json dictionary representing the joined_benchmark_config.

            Returns:
                    number_of_sample_slots (int): numberOfSampleSlots value for test event config
                    max_samples (int): maxSamples, which shall be used by clients on subscribe to test event.
    '''

    send_cycle_time_ms = joined_config_json["service_config"]["send_cycle_time_ms"]
    read_cycle_time_ms = joined_config_json["client_config"]["read_cycle_time_ms"]
    reader_count = joined_config_json["common"]["number_of_clients"]

    # this is the special case, where the client doesn't poll cyclically for new samples, but registers a receive-handler
    if (read_cycle_time_ms == 0):
        number_of_sample_slots = reader_count
        max_samples = 1
    else:
        sender_advantage = read_cycle_time_ms / send_cycle_time_ms
        max_samples = math.ceil(sender_advantage)
        number_of_sample_slots = reader_count * max_samples

    return number_of_sample_slots, max_samples


def create_client_benchmark_config(joined_config_json: dict, max_samples: int):
    '''
    Creates client benchmark app specific configuration out of the benchmark config (joined_benchmark_config)
    and the given max_samples

        Parameters:
                joined_config_json (dict): json dictionary representing the joined_benchmark_config.
                max_samples (int): Number of samples, with which the client app shall subscribe to test event.

        Returns:
                client benchmark config in form of a dict suitable to generate the expected json file from.
    '''

    client_config: dict = joined_config_json["client_config"]
    client_benchmark_config = dict()
    client_benchmark_config["number_of_clients"] = joined_config_json["common"]["number_of_clients"]
    client_benchmark_config["read_cycle_time_ms"] = client_config["read_cycle_time_ms"]
    client_benchmark_config["max_num_samples"] = max_samples
    client_benchmark_config["service_finder_mode"] = client_config["service_finder_mode"]

    run_time_limit = client_config.get("run_time_limit")
    if run_time_limit is not None:
        client_benchmark_config["run_time_limit"] = run_time_limit

    return client_benchmark_config


def create_client_mw_com_config(base_mw_com_config_json: dict, asil_level: str):
    '''
    Creates client benchmark app specific mw_com configuration out of the base mw_com_configuration.json
    and the given asil_level

        Parameters:
                base_mw_com_config_json (dict): json dictionary representing the base mw_com_configuration.json.
                asil_level (str): asil level, which shall be written into the mw_com_configuration

        Returns:
                mw_com_configuration in form of a dict suitable to generate the expected json file from.
    '''
    result = dict(base_mw_com_config_json)
    result["global"]["asil-level"] = asil_level
    result["serviceInstances"][0]["instances"][0]["asil-level"] = asil_level
    return result


def create_service_benchmark_config(joined_config_json: dict):
    '''
    Creates service benchmark app specific configuration out of the benchmark config (joined_benchmark_config)

        Parameters:
                joined_config_json (dict): json dictionary representing the joined_benchmark_config.

        Returns:
                service benchmark config in form of a dict suitable to generate the expected json file from.
    '''
    service_benchmark_config = dict()
    service_benchmark_config["number_of_clients"] = joined_config_json["common"]["number_of_clients"]
    service_benchmark_config["send_cycle_time_ms"] = joined_config_json["service_config"]["send_cycle_time_ms"]
    return service_benchmark_config


def create_service_mw_com_config(base_mw_com_config_json: dict, asil_level: str, number_of_sample_slots: int):
    '''
    Creates service benchmark app specific mw_com configuration out of the base mw_com_configuration.json
    and the given asil_level and number_of_sample_slots

        Parameters:
                base_mw_com_config_json (dict): json dictionary representing the base mw_com_configuration.json.
                asil_level (str): asil level, which shall be written into the mw_com_configuration
                number_of_sample_slots (int): numberOfSampleSlots, which shall be written into the mw_com_configuration
                                              for the test event.

        Returns:
                mw_com_configuration in form of a dict suitable to generate the expected json file from.
    '''
    result = dict(base_mw_com_config_json)
    result["global"]["asil-level"] = asil_level
    result["serviceInstances"][0]["instances"][0]["asil-level"] = asil_level
    result["serviceInstances"][0]["instances"][0]["events"][0]["numberOfSampleSlots"] = number_of_sample_slots
    return result


def main():
    # NOTE: at this point we expect that the joined_config.json
    # has been validated against the joined_benchmark_config_schema.json and
    # base_mw_com_config_path has been validated against ara_com_config_schema.json
    joined_config_json_path = str(sys.argv[1])
    base_mw_com_config_path = str(sys.argv[2])
    out_dir = str(sys.argv[3])

    joined_config_json = load_json(joined_config_json_path)
    base_mw_com_config_json = load_json(base_mw_com_config_path)

    numberOfSampleSlots, maxSamples = calculate_slot_numbers(joined_config_json)

    client_config_benchmark_json = create_client_benchmark_config(joined_config_json, maxSamples)
    save_json(f"{out_dir}/client_benchmark_config.json", client_config_benchmark_json)
    client_mw_com_config_json = create_client_mw_com_config(base_mw_com_config_json,
                                                            joined_config_json["common"]["asil_level"])
    save_json(f"{out_dir}/client_mw_com_config.json", client_mw_com_config_json)

    service_config_benchmark_json = create_service_benchmark_config(joined_config_json)
    save_json(f"{out_dir}/service_benchmark_config.json", service_config_benchmark_json)
    service_mw_com_config_json = create_service_mw_com_config(base_mw_com_config_json,
                                                              joined_config_json["common"]["asil_level"],
                                                              numberOfSampleSlots)
    save_json(f"{out_dir}/service_mw_com_config.json", service_mw_com_config_json)


if __name__ == "__main__":
    main()
