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
import pytest

logger = logging.getLogger(__name__)

def run_interaction_app(target, app_bin, mode, config_path, cwd, wait_on_exit=False, **kwargs):
    """Helper to run an application using the framework's native wrap_exec method."""
    args = ["--mode", mode, "--service_instance_manifest", config_path]
    return target.wrap_exec(app_bin, args, cwd=cwd, wait_on_exit=wait_on_exit, **kwargs)

@pytest.mark.xfail(reason="Generic Skeleton base pointer bug")
def test_generic_generic_interaction_64_byte(target):
    """
    Tests data validation for a 64-byte payload where both the 
    provider and consumer are type-erased generic interfaces.
    """
    app_root = "/opt/generic_generic_interaction_app/"
    app_bin = "./bin/generic_generic_interaction_64_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root):
        time.sleep(2) # Give provider a moment to initialize

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass

@pytest.mark.xfail(reason="Generic Skeleton base pointer bug")
def test_generic_generic_interaction_32_byte(target):
    """
    Tests data validation for a 32-byte payload where both the 
    provider and consumer are type-erased generic interfaces.
    """
    app_root = "/opt/generic_generic_interaction_app/"
    app_bin = "./bin/generic_generic_interaction_32_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root):
        time.sleep(2) # Give provider a moment to initialize

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass

@pytest.mark.xfail(reason="Generic Skeleton base pointer bug")
def test_generic_generic_interaction_16_byte(target):
    """
    Tests data validation for a 16-byte payload where both the 
    provider and consumer are type-erased generic interfaces.
    """
    app_root = "/opt/generic_generic_interaction_app/"
    app_bin = "./bin/generic_generic_interaction_16_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root):
        time.sleep(2) # Give provider a moment to initialize

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass

@pytest.mark.xfail(reason="Generic Skeleton base pointer bug")
def test_generic_generic_interaction_8_byte(target):
    """
    Tests data validation for an 8-byte payload where both the 
    provider and consumer are type-erased generic interfaces.
    """
    app_root = "/opt/generic_generic_interaction_app/"
    app_bin = "./bin/generic_generic_interaction_8_byte_app"
    config_path = "./etc/mw_com_config.json"

    logger.info(f"Starting provider: {app_bin} in {app_root}")
    with run_interaction_app(target, app_bin, "provider", config_path, cwd=app_root):
        time.sleep(2) # Give provider a moment to initialize

        logger.info(f"Starting consumer: {app_bin} in {app_root}")
        with run_interaction_app(target, app_bin, "consumer", config_path, cwd=app_root, wait_on_exit=True, wait_timeout=60):
            pass