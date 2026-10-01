/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_OSABSTRACTION_TEST_HELP_H
#define LG_SOMEIP_OSABSTRACTION_TEST_HELP_H

#include <cstddef>
#include <cstdint>
#include <string>

void create_msg(char* message, std::size_t max_length);
std::uint16_t get_test_port(std::uint16_t offset = 0);
std::string get_test_socket_path(const char* name);

#endif
