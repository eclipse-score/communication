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

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <exception/Exception.h>

using namespace lgsomeip;

TEST(ExceptionTest, ExceptionMessage) {
    int ret = 0;

    try {
        std::string msg{"MESSAGE"};
        throw LSAR_CONFIGURATION_ERROR(msg);
    } catch (const ConfigurationErrorException& e) {
        std::cout << e.what() << std::endl;
        if (std::string(e.what()).find("Configuration Error : MESSAGE") != std::string::npos)
            ret++;
    } catch (const std::exception& e) {
    }

    try {
        std::uint8_t code{0x01};
        throw LSAR_APPLICATION_ERROR(code);
    } catch (const ApplicationErrorException& e) {
        std::cout << e.what() << std::endl;
        if (std::string(e.what()).find("Application Error : Error Code(0x01)") != std::string::npos)
            ret++;
    } catch (const std::exception& e) {
    }

    try {
        std::string msg{"MESSAGE"};
        throw LSAR_SUBSCRIBE_ERROR(msg);
    } catch (const SubscribeErrorException& e) {
        std::cout << e.what() << std::endl;
        if (std::string(e.what()).find("Subscribe Error : MESSAGE") != std::string::npos)
            ret++;
    } catch (const std::exception& e) {
    }

    try {
        std::string msg{"MESSAGE"};
        throw LSAR_RUNTIME_ERROR(msg);
    } catch (const RuntimeErrorException& e) {
        std::cout << e.what() << std::endl;
        if (std::string(e.what()).find("Runtime Error : MESSAGE") != std::string::npos)
            ret++;
    } catch (const std::exception& e) {
    }

    EXPECT_EQ(ret, 4);
}
