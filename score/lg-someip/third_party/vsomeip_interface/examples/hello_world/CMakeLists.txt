# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.18)
project(vsomeip_hello_world)

find_package(Threads REQUIRED)
find_package(vsomeip CONFIG REQUIRED)

add_executable(hello_world_service hello_world_service.cpp)
target_compile_features(hello_world_service PRIVATE cxx_std_14)
target_link_libraries(hello_world_service PRIVATE vsomeip Threads::Threads)

add_executable(hello_world_client hello_world_client.cpp)
target_compile_features(hello_world_client PRIVATE cxx_std_14)
target_link_libraries(hello_world_client PRIVATE vsomeip Threads::Threads)
