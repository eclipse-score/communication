import json
import sys
import os
from typing import Dict


def load_json(json_path: str) -> Dict:
    with open(json_path, 'r') as json_file:
        loaded_json = json.load(json_file)
    return loaded_json


def save_json(json_path: str, json_data: Dict):
    os.makedirs(os.path.dirname(json_path), exist_ok=True)
    with open(json_path, "w") as json_file:
        json.dump(json_data, json_file, indent=4, sort_keys=True)


def create_mw_com_config(base_config_json: dict,
                         asil_level_app: str,
                         asil_level_communication_partner: str,
                         app_id: int | None):
    result = dict(base_config_json)
    result["global"]["asil-level"] = asil_level_app
    result["serviceInstances"][0]["instances"][0]["asil-level"] = asil_level_communication_partner
    if app_id is not None:
        result["global"]["applicationID"] = app_id
    return result


def parse_key(arg_dict, key):
    try:
        return arg_dict[key]
    except KeyError as e:
        print(f"json string must contain '{key}' key.")
        print(f"Error: {e}. Exiting.")
        sys.exit(1)


def main():
    base_mw_com_config_path = str(sys.argv[1])
    out_mw_com_config_path = str(sys.argv[2])
    try:
        arg_dict = json.loads(sys.argv[3])
    except json.decoder.JSONDecodeError as e:
        print("Third command line argument must be a valid json string.")
        print(f"Error: {e}. Exiting.")
        sys.exit(1)

    asil_level_app = parse_key(arg_dict, "asil-level-app")
    asil_level_communication_partner = parse_key(
        arg_dict, "asil-level-communication-partner")

    app_id_str = parse_key(arg_dict, "applicationID")
    if app_id_str == "":
        app_id = None
    else:
        try:
            app_id = int(app_id_str)
        except ValueError as e:
            print(
                "The value of 'applicationID' key must be either empty or a string that can be converted to an integer.")
            print(f"Error: {e}. Exiting.")
            sys.exit(1)

    base_config_json = load_json(base_mw_com_config_path)

    config_json = create_mw_com_config(
        base_config_json, asil_level_app, asil_level_communication_partner, app_id)
    save_json(out_mw_com_config_path, config_json)
    print("config saved")


if __name__ == "__main__":
    main()
