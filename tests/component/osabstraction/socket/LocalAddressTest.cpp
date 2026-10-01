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

#include <socket/LocalAddress.h>
#include <socket/TCPServerSocket.h>
#include <arpa/inet.h>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>

using namespace lgsomeip::osabstraction;

class ScopedUnixSocketFiles {
public:
    explicit ScopedUnixSocketFiles(const char* path) : path_(path) {}

    ~ScopedUnixSocketFiles() {
        std::remove(path_.c_str());
        std::remove((path_ + ".lock").c_str());
    }

private:
    std::string path_;
};

TEST(LocalAddress, set_file_path) {
    ScopedUnixSocketFiles cleanup("/tmp/uds222");
    std::shared_ptr<Address> addr = std::make_shared<LocalAddress>();
    char path[] = "/tmp/uds222";

    addr->set_file_path(path);
    TCPServerSocket local_socket(addr);

    EXPECT_EQ(access(path, F_OK), 0);
}

TEST(LocalAddress, get_file_path) {
    ScopedUnixSocketFiles cleanup("/tmp/uds111");
    std::shared_ptr<Address> addr = std::make_shared<LocalAddress>();
    char path[] = "/tmp/uds111";

    addr->set_file_path(path);
    TCPServerSocket local_socket(addr);

    EXPECT_EQ(access(path, F_OK), 0);
    EXPECT_EQ(addr->get_file_path(), path);
}
