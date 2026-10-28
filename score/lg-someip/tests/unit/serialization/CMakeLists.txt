# *******************************************************************************
# Copyright (C) 2017-2026 LG Electronics Inc.
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

cmake_minimum_required(VERSION 3.18)
project(lgsomeip_serialization_tests)

include(CTest)
find_package(GTest REQUIRED)

add_executable(unittest_serialization someip/SomeIPParameterSerializationTest.cpp)
target_include_directories(unittest_serialization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)
if(COMMAND lgsomeip_configure_target)
	lgsomeip_configure_target(unittest_serialization)
else()
	target_compile_features(unittest_serialization PRIVATE cxx_std_14)
	target_compile_options(unittest_serialization PRIVATE -Wall -pthread)
endif()

target_link_libraries(unittest_serialization PRIVATE GTest::gtest_main)

add_test(NAME serialization.unit COMMAND unittest_serialization)
set_tests_properties(serialization.unit PROPERTIES LABELS unit)

