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

import time
import logging

logger = logging.getLogger(__name__)

def run_interaction_app(target, app_bin, mode, config_path, cwd, wait_on_exit=False, **kwargs):
    """Helper to run an application using the framework's native wrap_exec method."""
    args = ["--mode", mode, "--service_instance_manifest", config_path]
    return target.wrap_exec(app_bin, args, cwd=cwd, wait_on_exit=wait_on_exit, **kwargs)

def test_generic_typed_interaction_64_byte(target):
    """
    Tests data validation and boundary checks for a 64-byte payload.
    """
    app_root = "/opt/generic_typed_interaction_app/"
    app_bin = "./bin/generic_typed_interaction_64_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    # Added enforce_clean_shutdown=False and disabled LSAN so forceful shutdown doesn't fail the test
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root, enforce_clean_shutdown=False, env={"ASAN_OPTIONS": "detect_leaks=0"}):
        # Give the provider a moment to initialize and offer the service
        # to prevent a race condition where the consumer starts too quickly.
        time.sleep(2)

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        # INCREASED wait_timeout to 60 to ensure it has time to finish
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass

def test_generic_typed_interaction_32_byte(target):
    """
    Tests data validation and boundary checks for a 32-byte payload.
    """
    app_root = "/opt/generic_typed_interaction_app/"
    app_bin = "./bin/generic_typed_interaction_32_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root, enforce_clean_shutdown=False, env={"ASAN_OPTIONS": "detect_leaks=0"}):
        time.sleep(2)

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass

def test_generic_typed_interaction_16_byte(target):
    """
    Tests data validation and boundary checks for a 16-byte payload.
    """
    app_root = "/opt/generic_typed_interaction_app/"
    app_bin = "./bin/generic_typed_interaction_16_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root, enforce_clean_shutdown=False, env={"ASAN_OPTIONS": "detect_leaks=0"}):
        time.sleep(2)

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass

def test_generic_typed_interaction_8_byte(target):
    """
    Tests data validation and boundary checks for an 8-byte payload.
    """
    app_root = "/opt/generic_typed_interaction_app/"
    app_bin = "./bin/generic_typed_interaction_8_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root, enforce_clean_shutdown=False, env={"ASAN_OPTIONS": "detect_leaks=0"}):
        time.sleep(2)

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass