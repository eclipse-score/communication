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
#include <memory>

#include <message/Message.h>
#include <message/Serializer.h>
#include <vector>
#include <string>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

struct ComplexData : public Enumerable {
    ComplexData() {}
    ComplexData(int first_value, int second_value) : a(first_value), b(second_value) {}
    int a;
    int b;

    void enumerate(Serializer& serializer) {
        serializer.push(a);
        serializer.push(b);
    }

    void enumerate(Deserializer& deserializer) {
        deserializer.pop(a);
        deserializer.pop(b);
    }

    std::uint32_t size() const {
        return 8;
    }
};

TEST(Serializer, Primitives1) {
    Serializer s;

    int a = 14, b = 15;
    s.push(a);
    s.push(b);

    std::vector<std::uint8_t> ret{0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0f};
    EXPECT_EQ(s.get_data(), ret);

    Deserializer ds;
    ds.set_data(s.get_data());

    int da, db;
    ds.pop(da);
    ds.pop(db);

    EXPECT_EQ(da, 14);
    EXPECT_EQ(db, 15);
}

TEST(Serializer, Primitives2) {
    Serializer s;

    int a = 14;
    double b = 100.0;
    s.push(a);
    s.push(b);

    std::vector<std::uint8_t> ret{0x00, 0x00, 0x00, 0x0e, 0x40, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(s.get_data(), ret);

    Deserializer ds;
    ds.set_data(s.get_data());

    int da;
    double db;
    ds.pop(da);
    ds.pop(db);

    EXPECT_EQ(da, 14);
    EXPECT_EQ(db, 100.0);
}

TEST(Serializer, DynamicArray) {
    Serializer s;

    std::vector<int> send{1, 2, 3};
    s.push(send);

    std::vector<std::uint8_t> ret{0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x01,
                                  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03};
    EXPECT_EQ(s.get_data(), ret);

    Deserializer ds;
    ds.set_data(s.get_data());

    std::vector<int> recv;
    ds.pop(recv);

    EXPECT_EQ(send, recv);
}

TEST(Serializer, DynamicLengthString) {
    Serializer s;

    std::string send = "ABCDEF";
    s.push<2>(send);

    std::vector<std::uint8_t> ret{0x00, 0x06, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    EXPECT_EQ(s.get_data(), ret);

    Deserializer ds;
    ds.set_data(s.get_data());

    std::string recv;
    ds.pop<2>(recv);

    EXPECT_EQ(send, recv);
}

TEST(SerializerTest, ComplexType1) {
    Serializer s;

    ComplexData send(14, 15);
    s.push(send);

    std::vector<std::uint8_t> ret{0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0f};
    EXPECT_EQ(s.get_data(), ret);

    Deserializer ds;
    ds.set_data(s.get_data());

    ComplexData recv;
    ds.pop(recv);

    EXPECT_EQ(send.a, recv.a);
    EXPECT_EQ(send.b, recv.b);
}

TEST(SerializerTest, ComplexType2) {
    Serializer s;

    std::vector<ComplexData> send;
    send.push_back(ComplexData(1, 2));
    send.push_back(ComplexData(3, 4));
    send.push_back(ComplexData(5, 6));
    s.push(send);

    std::vector<std::uint8_t> ret{0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
                                  0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06};

    EXPECT_EQ(s.get_data(), ret);

    Deserializer ds;
    ds.set_data(s.get_data());

    std::vector<ComplexData> recv;
    ds.pop(recv);

    EXPECT_EQ(send.size(), recv.size());

    bool check_data = true;
    for (std::size_t i = 0; i < send.size(); i++) {
        if (send[i].a != recv[i].a || send[i].b != recv[i].b)
            check_data = false;
    }

    EXPECT_EQ(check_data, true);
}
