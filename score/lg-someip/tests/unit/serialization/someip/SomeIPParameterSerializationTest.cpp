/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <gtest/gtest.h>
#include <serialization/someip/SomeIPParameterSerialization.h>

using namespace lgsomeip::serialization;

TEST(SerializerTest, is_host_network_byte_order) {
    unsigned int i = 1;
    ASSERT_EQ(!(*reinterpret_cast<char*>(&i) == 0x01), is_host_network_byte_order());
}

TEST(SerializerTest, NetworkByteOrder) {
    std::uint8_t d8 = 0x0F;
    ASSERT_EQ(0x0F, to_network_byte_order(d8));
    ASSERT_EQ(0x0F, from_network_byte_order(to_network_byte_order(d8)));

    std::uint16_t d16 = 0x2211;
    ASSERT_EQ(0x1122, to_network_byte_order(d16));
    ASSERT_EQ(d16, from_network_byte_order(to_network_byte_order(d16)));

    std::uint32_t d32 = 0x44332211;
    ASSERT_EQ(0x11223344, to_network_byte_order(d32));
    ASSERT_EQ(d32, from_network_byte_order(to_network_byte_order(d32)));
}

TEST(SerializerTest, SerializeBool) {
    bool value{true};

    Serializer serializer;
    serializer.push_back(value);
    ASSERT_EQ(1, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(1, buffer[0]);

    Deserializer deserializer{serializer.finish()};
    bool result{};
    deserializer.pop_front(result);
    ASSERT_EQ(true, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeUint8) {
    std::uint8_t value{0x11};

    Serializer serializer;
    serializer.push_back(value);
    ASSERT_EQ(1, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(0x11, buffer[0]);

    Deserializer deserializer{serializer.finish()};
    std::uint8_t result{};
    deserializer.pop_front(result);
    ASSERT_EQ(0x11, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeInt8) {
    std::int8_t value{0x11};

    Serializer serializer;
    serializer.push_back(value);
    ASSERT_EQ(1, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(0x11, buffer[0]);

    Deserializer deserializer{serializer.finish()};
    std::int8_t result{};
    deserializer.pop_front(result);
    ASSERT_EQ(0x11, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeUint16) {
    std::uint16_t value{0x2211};

    Serializer serializer;
    serializer.push_back(value);
    ASSERT_EQ(2, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(0x22, buffer[0]);
    ASSERT_EQ(0x11, buffer[1]);

    Deserializer deserializer{serializer.finish()};
    std::uint16_t result{};
    deserializer.pop_front(result);
    ASSERT_EQ(0x2211, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeInt16) {
    std::int16_t value{0x2211};

    Serializer serializer;
    serializer.push_back(value);
    ASSERT_EQ(2, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(0x22, buffer[0]);
    ASSERT_EQ(0x11, buffer[1]);

    Deserializer deserializer{serializer.finish()};
    std::int16_t result{};
    deserializer.pop_front(result);
    ASSERT_EQ(0x2211, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeUint32) {
    std::uint32_t value{0x44332211};

    Serializer serializer;
    serializer.push_back(value);
    ASSERT_EQ(4, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(0x44, buffer[0]);
    ASSERT_EQ(0x33, buffer[1]);
    ASSERT_EQ(0x22, buffer[2]);
    ASSERT_EQ(0x11, buffer[3]);

    Deserializer deserializer{serializer.finish()};
    std::uint32_t result{};
    deserializer.pop_front(result);
    ASSERT_EQ(0x44332211, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeInt32) {
    std::int32_t value{0x44332211};
    std::uint8_t* p = reinterpret_cast<std::uint8_t*>(&value);

    Serializer serializer;
    serializer.push_back(value);
    ASSERT_EQ(4, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(p[3], buffer[0]);
    ASSERT_EQ(p[2], buffer[1]);
    ASSERT_EQ(p[1], buffer[2]);
    ASSERT_EQ(p[0], buffer[3]);

    Deserializer deserializer{serializer.finish()};
    std::int32_t result{};
    deserializer.pop_front(result);
    ASSERT_EQ(0x44332211, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeFloat) {
    float f{3.14};
    std::uint8_t* p = reinterpret_cast<std::uint8_t*>(&f);

    Serializer serializer;
    serializer.push_back(f);
    ASSERT_EQ(4, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(p[3], buffer[0]);
    ASSERT_EQ(p[2], buffer[1]);
    ASSERT_EQ(p[1], buffer[2]);
    ASSERT_EQ(p[0], buffer[3]);

    Deserializer deserializer{serializer.finish()};
    float result{};
    deserializer.pop_front(result);
    ASSERT_EQ("3.140000", std::to_string(result));
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(SerializerTest, SerializeDouble) {
    double d{3.14};
    std::uint8_t* p = reinterpret_cast<std::uint8_t*>(&d);

    Serializer serializer;
    serializer.push_back(d);
    ASSERT_EQ(8, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(p[7], buffer[0]);
    ASSERT_EQ(p[6], buffer[1]);
    ASSERT_EQ(p[5], buffer[2]);
    ASSERT_EQ(p[4], buffer[3]);
    ASSERT_EQ(p[3], buffer[4]);
    ASSERT_EQ(p[2], buffer[5]);
    ASSERT_EQ(p[1], buffer[6]);
    ASSERT_EQ(p[0], buffer[7]);

    Deserializer deserializer{serializer.finish()};
    double result{};
    deserializer.pop_front(result);
    ASSERT_EQ(3.14, result);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

class SerializerInitTest : public ::testing::Test {
protected:
    virtual void SetUp() {}

    virtual void TearDown() {}

    Serializer serializer_;
};

TEST_F(SerializerInitTest, Initialization) {
    ASSERT_EQ(0, serializer_.get_length());
}

TEST_F(SerializerInitTest, SerializeData) {
    struct {
        std::uint8_t a;
        std::uint8_t b;
        double d;
    } data = {10, 20, 3.14};
    serializer_.push_back(data.a);
    serializer_.push_back(data.b);
    serializer_.push_back(data.d);
    ASSERT_EQ(10, serializer_.get_length());
    PacketBuffer& buffer = serializer_.get_buffer();
    ASSERT_EQ(10, buffer[0]);

    Deserializer deserializer{serializer_.finish()};
    std::uint8_t result1{};
    deserializer.pop_front(result1);
    ASSERT_EQ(10, result1);
    std::uint8_t result2{};
    deserializer.pop_front(result2);
    ASSERT_EQ(20, result2);
    double result3{};
    deserializer.pop_front(result3);
    ASSERT_EQ(3.14, result3);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(ComplexTypeSerializerTest, NoLengthConfig) {
    struct Data {
        std::uint8_t a;
        std::uint8_t b;
        double d;
    };

    Data data = {10, 20, 3.14};

    Serializer serializer;
    ComplexTypeSerializer<LengthFieldConfig<void>> complex_serializer{&serializer};
    complex_serializer.push_back(data.a);
    complex_serializer.push_back(data.b);
    complex_serializer.push_back(data.d);
    complex_serializer.finish();

    ASSERT_EQ(10, serializer.get_length());
    PacketBuffer& buffer = serializer.get_buffer();
    ASSERT_EQ(10, buffer[0]);

    Deserializer deserializer(serializer.finish());
    ComplexTypeDeserializer<LengthFieldConfig<void>> complex_deserializer{&deserializer};
    complex_deserializer.pop_front(data.a);
    complex_deserializer.pop_front(data.b);
    complex_deserializer.pop_front(data.d);
    ASSERT_EQ(10, data.a);
    ASSERT_EQ(20, data.b);
    ASSERT_EQ(3.14, data.d);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(ComplexTypeSerializerTest, OneByteLengthConfig) {
    Serializer serializer;
    PacketBuffer& buffer = serializer.get_buffer();

    struct Data {
        std::uint8_t a;
        std::uint8_t b;
        double d;
    };
    Data data = {10, 20, 3.14};
    ComplexTypeSerializer<LengthFieldConfig<std::uint8_t>> complex_serializer{&serializer};
    ASSERT_EQ(1, serializer.get_length());
    ASSERT_EQ(0, buffer[0]);
    complex_serializer.push_back(data.a);
    complex_serializer.push_back(data.b);
    complex_serializer.push_back(data.d);
    complex_serializer.finish();

    ASSERT_EQ(11, serializer.get_length());
    ASSERT_EQ(10, buffer[0]);

    Deserializer deserializer(serializer.finish());
    ComplexTypeDeserializer<LengthFieldConfig<std::uint8_t>> complex_deserializer{&deserializer};
    complex_deserializer.pop_front(data.a);
    complex_deserializer.pop_front(data.b);
    complex_deserializer.pop_front(data.d);
    ASSERT_EQ(10, data.a);
    ASSERT_EQ(20, data.b);
    ASSERT_EQ(3.14, data.d);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(ComplexTypeSerializerTest, TwoByteLengthConfig) {
    Serializer serializer;
    PacketBuffer& buffer = serializer.get_buffer();

    struct {
        std::uint8_t a;
        std::uint8_t b;
        double d;
    } data = {10, 20, 3.14};
    ComplexTypeSerializer<LengthFieldConfig<std::uint16_t>> complex_serializer{&serializer};
    ASSERT_EQ(2, serializer.get_length());

    complex_serializer.push_back(data.a);
    complex_serializer.push_back(data.b);
    complex_serializer.push_back(data.d);
    complex_serializer.finish();
    ASSERT_EQ(12, serializer.get_length());
    ASSERT_EQ(10, buffer[1]);

    Deserializer deserializer(serializer.finish());
    ComplexTypeDeserializer<LengthFieldConfig<std::uint16_t>> complex_deserializer{&deserializer};
    complex_deserializer.pop_front(data.a);
    complex_deserializer.pop_front(data.b);
    complex_deserializer.pop_front(data.d);
    ASSERT_EQ(10, data.a);
    ASSERT_EQ(20, data.b);
    ASSERT_EQ(3.14, data.d);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(ComplexTypeSerializerTest, FourByteLengthConfig) {
    Serializer serializer;
    PacketBuffer& buffer = serializer.get_buffer();

    struct {
        std::uint8_t a;
        std::uint8_t b;
        double d;
    } data = {10, 20, 3.14};
    ComplexTypeSerializer<LengthFieldConfig<std::uint32_t>> complex_serializer{&serializer};
    ASSERT_EQ(4, serializer.get_length());

    complex_serializer.push_back(data.a);
    complex_serializer.push_back(data.b);
    complex_serializer.push_back(data.d);
    complex_serializer.finish();

    ASSERT_EQ(14, serializer.get_length());
    ASSERT_EQ(10, buffer[3]);

    Deserializer deserializer(serializer.finish());
    ComplexTypeDeserializer<LengthFieldConfig<std::uint32_t>> complex_deserializer{&deserializer};
    complex_deserializer.pop_front(data.a);
    complex_deserializer.pop_front(data.b);
    complex_deserializer.pop_front(data.d);
    ASSERT_EQ(10, data.a);
    ASSERT_EQ(20, data.b);
    ASSERT_EQ(3.14, data.d);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(TlvSerializerTest, SerializeStructure) {
    const std::uint32_t data_id_a = 0;
    const std::uint32_t data_id_c = 2;
    const std::uint32_t data_id_cc1 = 1;
    struct {
        std::uint8_t a{};       // Data ID = 0, optional = false
        std::uint8_t b{};       // Data ID = 1, optional = false
        struct {                // Data ID = 2, optional = false
            std::uint8_t c1{};  // Data ID = 1, optional = false
            std::uint32_t c2{}; // Data ID = 0, optional = false
        } c;
    } data;

    data.a = 11;
    data.c.c1 = 33;

    Serializer serializer;
    PacketBuffer& buffer = serializer.get_buffer();
    ComplexTypeSerializer<LengthFieldConfig<std::uint32_t>> complex_serializer{&serializer};
    ASSERT_EQ(4, serializer.get_length());

    {
        TlvSerializer<LengthFieldConfig<std::uint32_t>> tlv_serializer_data_a{&serializer, WireTypeEnum::BaseType8Bit,
                                                                              data_id_a};
        tlv_serializer_data_a.push_back(data.a);
        tlv_serializer_data_a.finish();
    }

    {
        TlvSerializer<LengthFieldConfig<std::uint32_t>> tlv_serializer_data_c{
            &serializer, WireTypeEnum::ComplexDataTypeFromDataDefinition, data_id_c};
        {
            TlvSerializer<LengthFieldConfig<std::uint32_t>> tlv_serializer_data_c_c1{
                &serializer, WireTypeEnum::BaseType8Bit, data_id_cc1};
            tlv_serializer_data_c_c1.push_back(data.c.c1);
            tlv_serializer_data_c_c1.finish();
        }
        tlv_serializer_data_c.finish();
    }

    complex_serializer.finish();

    ASSERT_EQ(16, serializer.get_length());

    std::uint32_t length1 = buffer[0];
    length1 = length1 << 8 | buffer[1];
    length1 = length1 << 8 | buffer[2];
    length1 = length1 << 8 | buffer[3];
    ASSERT_EQ(12, length1);

    ASSERT_EQ(data_id_a, buffer[5]); // data id = 0
    ASSERT_EQ(11, buffer[6]);        // value 11

    ASSERT_EQ(0x40, buffer[7] & 0x70); // Complext Data Type from data definition
    ASSERT_EQ(data_id_c, buffer[8]);   // data id = 2
    std::uint32_t length2 = buffer[9];
    length2 = length2 << 8 | buffer[10];
    length2 = length2 << 8 | buffer[11];
    length2 = length2 << 8 | buffer[12];

    ASSERT_EQ(33, buffer[15]); // data id = 2

    Deserializer deserializer(serializer.finish());
    ComplexTypeDeserializer<LengthFieldConfig<std::uint32_t>> complex_deserializer{&deserializer};

    int members = 0;
    while (complex_deserializer.has_remaining_data()) {
        ASSERT_LT(members, 2);
        TlvDeserializer<LengthFieldConfig<std::uint32_t>> tlv_deserializer_data{&deserializer, &complex_deserializer};
        TlvTag tag = tlv_deserializer_data.consume_tag();
        switch (tag.data_id_) {
        case data_id_a: {
            std::uint8_t d{};
            ASSERT_EQ(1, tlv_deserializer_data.pop_front(d));
            ASSERT_EQ(11, d);
            break;
        }
        case data_id_c: {
            // The nested member reports what it reads to the member that contains it.
            TlvDeserializer<LengthFieldConfig<std::uint32_t>> tlv_deserializer_data_c{&deserializer,
                                                                                      &tlv_deserializer_data};
            TlvTag inner_tag = tlv_deserializer_data_c.consume_tag();
            ASSERT_EQ(data_id_cc1, inner_tag.data_id_);
            std::uint8_t d{};
            ASSERT_EQ(1, tlv_deserializer_data_c.pop_front(d));
            ASSERT_EQ(33, d);
            break;
        }
        default:
            FAIL() << "unexpected data id " << tag.data_id_;
        }
        members++;
    }

    ASSERT_EQ(2, members);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(TlvSerializerTest, DecodesWireTypeAndDataIdAbove255) {
    const std::uint16_t data_id = 0x0A23;
    const std::uint32_t expected_value = 0xCAFEBABEU;

    Serializer serializer;
    {
        TlvSerializer<LengthFieldConfig<std::uint32_t>> tlv_serializer{&serializer, WireTypeEnum::BaseType32Bit,
                                                                       data_id};
        tlv_serializer.push_back(expected_value);
        tlv_serializer.finish();
    }

    Deserializer deserializer(serializer.finish());
    TlvDeserializer<LengthFieldConfig<std::uint32_t>> tlv_deserializer{&deserializer};

    TlvTag tag = tlv_deserializer.consume_tag();
    ASSERT_EQ(data_id, tag.data_id_);
    ASSERT_EQ(4U, tag.len_);

    std::uint32_t value{};
    tlv_deserializer.pop_front(value);
    ASSERT_EQ(expected_value, value);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(DeserializerTest, TruncatedInputReturnsZeroAndKeepsPosition) {
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x01, 0x02, 0x03})};

    std::uint32_t word{};
    ASSERT_EQ(0, deserializer.pop_front(word));
    ASSERT_EQ(3, deserializer.get_remaining_length());

    std::uint16_t half{};
    ASSERT_EQ(2, deserializer.pop_front(half));
    ASSERT_EQ(0x0102, half);
    ASSERT_EQ(1, deserializer.get_remaining_length());

    ASSERT_EQ(0, deserializer.pop_front(half));
    ASSERT_EQ(1, deserializer.get_remaining_length());
}

TEST(ComplexTypeDeserializerTest, LengthFieldLargerThanBuffer) {
    // The length field announces 8 bytes but only 2 follow.
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x00, 0x00, 0x00, 0x08, 0x01, 0x02})};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint32_t>> complex_deserializer{&deserializer};

    std::uint8_t byte{};
    ASSERT_EQ(1, complex_deserializer.pop_front(byte));
    ASSERT_EQ(1, complex_deserializer.pop_front(byte));
    ASSERT_EQ(0, complex_deserializer.pop_front(byte));
    // The announced data never arrives, so callers must stop when pop_front() returns 0.
    ASSERT_TRUE(complex_deserializer.has_remaining_data());
}

TEST(ComplexTypeDeserializerTest, MissingLengthField) {
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x00, 0x00})};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint32_t>> complex_deserializer{&deserializer};

    std::uint8_t byte{};
    ASSERT_EQ(0, complex_deserializer.pop_front(byte));
    ASSERT_FALSE(complex_deserializer.has_remaining_data());
}

TEST(TlvDeserializerTest, TruncatedMember) {
    // 32-bit base type member whose value is cut off after two bytes.
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x20, 0x01, 0x11, 0x22})};
    TlvDeserializer<LengthFieldConfig<std::uint16_t>> tlv_deserializer{&deserializer};

    TlvTag tag = tlv_deserializer.consume_tag();
    ASSERT_EQ(1, tag.data_id_);
    ASSERT_EQ(4, tag.len_);

    std::uint32_t value{};
    ASSERT_EQ(0, tlv_deserializer.pop_front(value));
    ASSERT_EQ(2, deserializer.get_remaining_length());
}

TEST(DeserializerTest, BooleanUsesOnlyLowestBit) {
    // feat_req_someip_817: the other bits are reserved and shall be ignored on receiving.
    const PacketBuffer input{0x00, 0x01, 0x02, 0x03, 0x80, 0xFF};
    const bool expected[] = {false, true, false, true, false, true};

    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer(input))};
    for (const bool want : expected) {
        bool value = !want;
        ASSERT_EQ(1, deserializer.pop_front(value));
        ASSERT_EQ(want, value);
    }
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(TlvDeserializerTest, HasNoDataBeforeTheFirstTagIsRead) {
    // Construct the object over memory filled with a pattern, as happens for a local variable on a used stack.
    alignas(TlvDeserializer<LengthFieldConfig<std::uint16_t>>) unsigned char
        storage[sizeof(TlvDeserializer<LengthFieldConfig<std::uint16_t>>)];
    std::memset(storage, 0x55, sizeof(storage));

    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x20, 0x01, 0x11, 0x22, 0x33, 0x44})};
    auto* tlv_deserializer = new (storage) TlvDeserializer<LengthFieldConfig<std::uint16_t>>(&deserializer);
    ASSERT_FALSE(tlv_deserializer->has_remaining_data());
    tlv_deserializer->~TlvDeserializer();
}

TEST(ComplexTypeDeserializerTest, NoLengthFieldHasNoAnnouncedData) {
    alignas(ComplexTypeDeserializer<LengthFieldConfig<void>>) unsigned char
        storage[sizeof(ComplexTypeDeserializer<LengthFieldConfig<void>>)];
    std::memset(storage, 0x55, sizeof(storage));

    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x01, 0x02})};
    auto* complex_deserializer = new (storage) ComplexTypeDeserializer<LengthFieldConfig<void>>(&deserializer);
    ASSERT_FALSE(complex_deserializer->has_remaining_data());
    complex_deserializer->~ComplexTypeDeserializer();
}

TEST(ComplexTypeSerializerTest, LengthFieldOverflowIsReported) {
    {
        Serializer serializer;
        ComplexTypeSerializer<LengthFieldConfig<std::uint8_t>> complex_serializer{&serializer};
        for (int i = 0; i < 255; i++) {
            complex_serializer.push_back(static_cast<std::uint8_t>(i));
        }
        ASSERT_TRUE(complex_serializer.finish());
        ASSERT_EQ(255, serializer.get_buffer()[0]);
    }
    {
        Serializer serializer;
        ComplexTypeSerializer<LengthFieldConfig<std::uint8_t>> complex_serializer{&serializer};
        for (int i = 0; i < 256; i++) {
            complex_serializer.push_back(static_cast<std::uint8_t>(i));
        }
        // 256 bytes do not fit into an 8 bit length field; the field must not claim a wrong size.
        ASSERT_FALSE(complex_serializer.finish());
        ASSERT_EQ(0, serializer.get_buffer()[0]);
    }
    {
        Serializer serializer;
        ComplexTypeSerializer<LengthFieldConfig<std::uint16_t>> complex_serializer{&serializer};
        for (std::uint32_t i = 0; i < 0x10000U; i++) {
            complex_serializer.push_back(static_cast<std::uint8_t>(i));
        }
        ASSERT_FALSE(complex_serializer.finish());
    }
    {
        Serializer serializer;
        ComplexTypeSerializer<LengthFieldConfig<void>> complex_serializer{&serializer};
        complex_serializer.push_back(static_cast<std::uint8_t>(1));
        ASSERT_TRUE(complex_serializer.finish());
    }
}

TEST(TlvSerializerTest, LengthFieldOverflowIsReported) {
    Serializer serializer;
    TlvSerializer<LengthFieldConfig<std::uint16_t>> tlv_serializer{&serializer,
                                                                   WireTypeEnum::ComplexDataType8BitLengthField, 1};
    for (int i = 0; i < 256; i++) {
        tlv_serializer.push_back(static_cast<std::uint8_t>(i));
    }
    ASSERT_FALSE(tlv_serializer.finish());

    Serializer fitting;
    TlvSerializer<LengthFieldConfig<std::uint16_t>> fitting_serializer{&fitting,
                                                                       WireTypeEnum::ComplexDataType8BitLengthField, 1};
    fitting_serializer.push_back(static_cast<std::uint8_t>(7));
    ASSERT_TRUE(fitting_serializer.finish());

    Serializer base_type;
    TlvSerializer<LengthFieldConfig<std::uint16_t>> base_serializer{&base_type, WireTypeEnum::BaseType8Bit, 1};
    base_serializer.push_back(static_cast<std::uint8_t>(7));
    ASSERT_TRUE(base_serializer.finish());
}

TEST(DeserializerTest, SkipBytes) {
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x01, 0x02, 0x03, 0x04})};

    ASSERT_EQ(0, deserializer.skip(5));
    ASSERT_EQ(4, deserializer.get_remaining_length());

    ASSERT_EQ(3, deserializer.skip(3));
    ASSERT_EQ(1, deserializer.get_remaining_length());

    std::uint8_t value{};
    ASSERT_EQ(1, deserializer.pop_front(value));
    ASSERT_EQ(0x04, value);
}

TEST(ComplexTypeDeserializerTest, SkipsBytesOfAnExtendedStruct) {
    // feat_req_someip_601: a struct longer than the known definition carries extra bytes that are skipped.
    // The struct knows one uint8; the sender added three more bytes. 0xAA follows the struct.
    Deserializer deserializer{
        std::unique_ptr<PacketBuffer>(new PacketBuffer{0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 0xAA})};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint16_t>> complex_deserializer{&deserializer};

    std::uint8_t value{};
    ASSERT_EQ(1, complex_deserializer.pop_front(value));
    ASSERT_EQ(0x01, value);
    ASSERT_TRUE(complex_deserializer.has_remaining_data());

    ASSERT_TRUE(complex_deserializer.skip_remaining());
    ASSERT_FALSE(complex_deserializer.has_remaining_data());

    ASSERT_EQ(1, deserializer.pop_front(value));
    ASSERT_EQ(0xAA, value);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(ComplexTypeDeserializerTest, SkipFailsWhenTheAnnouncedBytesAreMissing) {
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x00, 0x08, 0x01, 0x02})};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint16_t>> complex_deserializer{&deserializer};

    std::uint8_t value{};
    ASSERT_EQ(1, complex_deserializer.pop_front(value));
    ASSERT_FALSE(complex_deserializer.skip_remaining());
    ASSERT_EQ(1, deserializer.get_remaining_length());
}

TEST(ComplexTypeDeserializerTest, SkipWithoutLengthFieldDoesNothing) {
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x01, 0x02})};
    ComplexTypeDeserializer<LengthFieldConfig<void>> complex_deserializer{&deserializer};

    ASSERT_TRUE(complex_deserializer.skip_remaining());
    ASSERT_EQ(2, deserializer.get_remaining_length());
}

TEST(ComplexTypeDeserializerTest, SkipInsideANestedStruct) {
    // Outer (8 bit length) { Inner (8 bit length) { u8 known, 2 unknown bytes }, u8 0x09 }
    Deserializer deserializer{std::unique_ptr<PacketBuffer>(new PacketBuffer{0x05, 0x03, 0x01, 0xEE, 0xEF, 0x09})};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint8_t>> outer{&deserializer};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint8_t>> inner{&deserializer};

    std::uint8_t value{};
    ASSERT_EQ(1, inner.pop_front(value));
    ASSERT_EQ(0x01, value);
    ASSERT_TRUE(inner.skip_remaining());
    outer.consumed(1 + 3);

    ASSERT_EQ(1, outer.pop_front(value));
    ASSERT_EQ(0x09, value);
    ASSERT_FALSE(outer.has_remaining_data());
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(TlvDeserializerTest, SkipsUnknownMember) {
    // An unknown member (data id 5, 8 bit length field, 3 bytes) in front of a known uint8 member (data id 1).
    Deserializer deserializer{
        std::unique_ptr<PacketBuffer>(new PacketBuffer{0x50, 0x05, 0x03, 0xA1, 0xA2, 0xA3, 0x00, 0x01, 0x07})};

    TlvDeserializer<LengthFieldConfig<std::uint16_t>> unknown{&deserializer};
    TlvTag tag = unknown.consume_tag();
    ASSERT_EQ(5, tag.data_id_);
    ASSERT_EQ(3, tag.len_);
    ASSERT_TRUE(unknown.skip_remaining());
    ASSERT_FALSE(unknown.has_remaining_data());

    TlvDeserializer<LengthFieldConfig<std::uint16_t>> known{&deserializer};
    tag = known.consume_tag();
    ASSERT_EQ(1, tag.data_id_);
    std::uint8_t value{};
    ASSERT_EQ(1, known.pop_front(value));
    ASSERT_EQ(7, value);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(TlvDeserializerTest, SkipReportsBytesToTheContainingStruct) {
    // Struct (8 bit length, 8 bytes) { unknown member (5 bytes), known uint8 member (3 bytes) }
    Deserializer deserializer{
        std::unique_ptr<PacketBuffer>(new PacketBuffer{0x08, 0x50, 0x05, 0x02, 0xA1, 0xA2, 0x00, 0x01, 0x07})};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint8_t>> complex_deserializer{&deserializer};

    int members = 0;
    std::uint8_t value{};
    while (complex_deserializer.has_remaining_data()) {
        ASSERT_LT(members, 2);
        TlvDeserializer<LengthFieldConfig<std::uint16_t>> tlv{&deserializer, &complex_deserializer};
        TlvTag tag = tlv.consume_tag();
        if (tag.data_id_ == 1) {
            ASSERT_EQ(1, tlv.pop_front(value));
        } else {
            ASSERT_TRUE(tlv.skip_remaining());
        }
        members++;
    }
    ASSERT_EQ(2, members);
    ASSERT_EQ(7, value);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

namespace {

PacketBuffer bytes_of(Serializer& serializer) {
    return *serializer.finish();
}

std::unique_ptr<PacketBuffer> buffer_of(const PacketBuffer& bytes) {
    return std::unique_ptr<PacketBuffer>(new PacketBuffer(bytes));
}

} // namespace

TEST(StringTest, DynamicUtf8) {
    Serializer serializer;
    ASSERT_TRUE(push_back_string(serializer, std::string("Hi")));
    // 32 bit length (6), BOM, "Hi", terminating zero; the length field does not count itself.
    const PacketBuffer golden{0x00, 0x00, 0x00, 0x06, 0xEF, 0xBB, 0xBF, 0x48, 0x69, 0x00};
    PacketBuffer encoded = bytes_of(serializer);
    ASSERT_EQ(golden, encoded);

    Deserializer deserializer{buffer_of(golden)};
    std::string text;
    ASSERT_EQ(golden.size(), pop_front_string(deserializer, text));
    ASSERT_EQ("Hi", text);
    ASSERT_EQ(0, deserializer.get_remaining_length());
}

TEST(StringTest, DynamicUtf8KoreanAndEmpty) {
    const std::string korean = "\xED\x95\x9C\xEA\xB8\x80";
    Serializer serializer;
    ASSERT_TRUE(push_back_string(serializer, korean));
    ASSERT_TRUE(push_back_string(serializer, std::string()));
    const PacketBuffer golden{0x00, 0x00, 0x00, 0x0A, 0xEF, 0xBB, 0xBF, 0xED, 0x95, 0x9C, 0xEA,
                              0xB8, 0x80, 0x00, 0x00, 0x00, 0x00, 0x04, 0xEF, 0xBB, 0xBF, 0x00};
    ASSERT_EQ(golden, bytes_of(serializer));

    Deserializer deserializer{buffer_of(golden)};
    std::string text;
    ASSERT_EQ(14, pop_front_string(deserializer, text));
    ASSERT_EQ(korean, text);
    ASSERT_EQ(8, pop_front_string(deserializer, text));
    ASSERT_EQ("", text);
}

TEST(StringTest, DynamicUtf16) {
    const std::u16string text16 = u"Hi";
    {
        Serializer serializer;
        ASSERT_TRUE(push_back_string(serializer, text16, StringEncoding::Utf16BE));
        const PacketBuffer golden{0x00, 0x00, 0x00, 0x08, 0xFE, 0xFF, 0x00, 0x48, 0x00, 0x69, 0x00, 0x00};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        std::u16string text;
        ASSERT_EQ(golden.size(), pop_front_string(deserializer, text, StringEncoding::Utf16BE));
        ASSERT_EQ(text16, text);
    }
    {
        Serializer serializer;
        ASSERT_TRUE(push_back_string(serializer, text16, StringEncoding::Utf16LE));
        const PacketBuffer golden{0x00, 0x00, 0x00, 0x08, 0xFF, 0xFE, 0x48, 0x00, 0x69, 0x00, 0x00, 0x00};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        std::u16string text;
        ASSERT_EQ(golden.size(), pop_front_string(deserializer, text, StringEncoding::Utf16LE));
        ASSERT_EQ(text16, text);
    }
}

TEST(StringTest, LengthFieldWidth) {
    Serializer serializer;
    ASSERT_TRUE(push_back_string<LengthFieldConfig<std::uint8_t>>(serializer, std::string("Hi")));
    ASSERT_TRUE(push_back_string<LengthFieldConfig<std::uint16_t>>(serializer, std::string("Hi")));
    const PacketBuffer golden{0x06, 0xEF, 0xBB, 0xBF, 0x48, 0x69, 0x00, 0x00, 0x06, 0xEF, 0xBB, 0xBF, 0x48, 0x69, 0x00};
    ASSERT_EQ(golden, bytes_of(serializer));

    Deserializer deserializer{buffer_of(golden)};
    std::string text;
    ASSERT_EQ(7, pop_front_string<LengthFieldConfig<std::uint8_t>>(deserializer, text));
    ASSERT_EQ("Hi", text);
    ASSERT_EQ(8, pop_front_string<LengthFieldConfig<std::uint16_t>>(deserializer, text));
    ASSERT_EQ("Hi", text);
}

TEST(StringTest, LengthFieldOverflow) {
    // BOM and terminator take 4 bytes; 251 characters make 255, which is the largest 8 bit length.
    Serializer fitting;
    ASSERT_TRUE(push_back_string<LengthFieldConfig<std::uint8_t>>(fitting, std::string(251, 'a')));
    ASSERT_EQ(255, fitting.get_buffer()[0]);

    Serializer too_long;
    ASSERT_FALSE(push_back_string<LengthFieldConfig<std::uint8_t>>(too_long, std::string(252, 'a')));
}

TEST(StringTest, EmbeddedZeroIsRejected) {
    Serializer serializer;
    ASSERT_FALSE(push_back_string(serializer, std::string("a\0b", 3)));
    ASSERT_FALSE(push_back_string(serializer, std::u16string(u"a\0b", 3), StringEncoding::Utf16BE));
    ASSERT_FALSE(push_back_string(serializer, std::u16string(u"a"), StringEncoding::Utf8));
}

TEST(StringTest, FixedLength) {
    {
        // 8 bytes: BOM, "Hi", terminator, unused space filled with zeros.
        Serializer serializer;
        ASSERT_TRUE(push_back_fixed_string(serializer, 8, std::string("Hi")));
        const PacketBuffer golden{0xEF, 0xBB, 0xBF, 0x48, 0x69, 0x00, 0x00, 0x00};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        std::string text;
        ASSERT_EQ(8, pop_front_fixed_string(deserializer, 8, text));
        ASSERT_EQ("Hi", text);
    }
    {
        Serializer serializer;
        ASSERT_TRUE(push_back_fixed_string(serializer, 10, std::u16string(u"Hi"), StringEncoding::Utf16BE));
        const PacketBuffer golden{0xFE, 0xFF, 0x00, 0x48, 0x00, 0x69, 0x00, 0x00, 0x00, 0x00};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        std::u16string text;
        ASSERT_EQ(10, pop_front_fixed_string(deserializer, 10, text, StringEncoding::Utf16BE));
        ASSERT_EQ(u"Hi", text);
    }
    {
        Serializer serializer;
        // BOM, "Hi" and the terminator need 6 bytes.
        ASSERT_FALSE(push_back_fixed_string(serializer, 5, std::string("Hi")));
        // UTF-16 strings have an even length.
        ASSERT_FALSE(push_back_fixed_string(serializer, 9, std::u16string(u"Hi"), StringEncoding::Utf16LE));
        ASSERT_EQ(0, serializer.get_length());
    }
}

TEST(StringTest, PaddingBehindTheTerminatorIsIgnored) {
    // feat_req_someip_239: a string may be extended with zeros to reach an alignment.
    const PacketBuffer input{0x00, 0x00, 0x00, 0x08, 0xEF, 0xBB, 0xBF, 0x48, 0x69, 0x00, 0x00, 0x00};
    Deserializer deserializer{buffer_of(input)};
    std::string text;
    ASSERT_EQ(12, pop_front_string(deserializer, text));
    ASSERT_EQ("Hi", text);
}

TEST(StringTest, OddUtf16LengthIgnoresTheLastByte) {
    // feat_req_someip_641: BOM, "H", terminator, and one extra byte that is ignored.
    const PacketBuffer input{0x00, 0x00, 0x00, 0x07, 0xFE, 0xFF, 0x00, 0x48, 0x00, 0x00, 0x99};
    Deserializer deserializer{buffer_of(input)};
    std::u16string text;
    ASSERT_EQ(11, pop_front_string(deserializer, text, StringEncoding::Utf16BE));
    ASSERT_EQ(u"H", text);
}

TEST(StringTest, MalformedStringsAreRejected) {
    const PacketBuffer cases[] = {
        {0x00, 0x00, 0x00, 0x03, 0x48, 0x69, 0x00},                         // no BOM
        {0x00, 0x00, 0x00, 0x05, 0xEF, 0xBB, 0xBF, 0x48, 0x69},             // no terminator
        {0x00, 0x00, 0x00, 0x08, 0xEF, 0xBB, 0xBF, 0x48, 0x69, 0x00},       // length exceeds the data
        {0x00, 0x00, 0x00, 0x07, 0xEF, 0xBB, 0xBF, 0x48, 0x00, 0x69, 0x00}, // data behind the terminator
        {0x00, 0x00, 0x00},                                                 // length field cut off
    };
    for (const auto& input : cases) {
        Deserializer deserializer{buffer_of(input)};
        std::string text;
        ASSERT_EQ(0, pop_front_string(deserializer, text));
    }

    // The BOM must match the requested encoding (feat_req_someip_666).
    const PacketBuffer utf16le{0x00, 0x00, 0x00, 0x06, 0xFF, 0xFE, 0x48, 0x00, 0x00, 0x00};
    Deserializer deserializer{buffer_of(utf16le)};
    std::u16string text;
    ASSERT_EQ(0, pop_front_string(deserializer, text, StringEncoding::Utf16BE));
}

TEST(StringTest, StringInsideAStruct) {
    // Struct (16 bit length) { uint8 1, string (8 bit length) "Hi", uint8 2 }
    Serializer serializer;
    ComplexTypeSerializer<LengthFieldConfig<std::uint16_t>> complex_serializer{&serializer};
    complex_serializer.push_back(static_cast<std::uint8_t>(1));
    ASSERT_TRUE(push_back_string<LengthFieldConfig<std::uint8_t>>(serializer, std::string("Hi")));
    complex_serializer.push_back(static_cast<std::uint8_t>(2));
    ASSERT_TRUE(complex_serializer.finish());
    const PacketBuffer golden{0x00, 0x09, 0x01, 0x06, 0xEF, 0xBB, 0xBF, 0x48, 0x69, 0x00, 0x02};
    ASSERT_EQ(golden, bytes_of(serializer));

    Deserializer deserializer{buffer_of(golden)};
    ComplexTypeDeserializer<LengthFieldConfig<std::uint16_t>> complex_deserializer{&deserializer};
    std::uint8_t value{};
    std::string text;
    ASSERT_EQ(1, complex_deserializer.pop_front(value));
    const std::size_t consumed = pop_front_string<LengthFieldConfig<std::uint8_t>>(deserializer, text);
    ASSERT_EQ(7, consumed);
    complex_deserializer.consumed(consumed);
    ASSERT_EQ("Hi", text);
    ASSERT_EQ(1, complex_deserializer.pop_front(value));
    ASSERT_EQ(2, value);
    ASSERT_FALSE(complex_deserializer.has_remaining_data());
}

TEST(UnionTest, SpecificationExample) {
    // Union of uint8 (type 1) and uint16 (type 2): the shorter element is padded to the longest one.
    {
        Serializer serializer;
        UnionSerializer<> union_serializer{&serializer, 1};
        union_serializer.push_back(static_cast<std::uint8_t>(0xAB));
        ASSERT_TRUE(union_serializer.finish(4));
        const PacketBuffer golden{0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0xAB, 0x00, 0x00, 0x00};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        UnionDeserializer<> union_deserializer{&deserializer};
        ASSERT_TRUE(union_deserializer.valid());
        ASSERT_EQ(1U, union_deserializer.type());
        std::uint8_t value{};
        ASSERT_EQ(1, union_deserializer.pop_front(value));
        ASSERT_EQ(0xAB, value);
        ASSERT_TRUE(union_deserializer.has_remaining_data());
        ASSERT_TRUE(union_deserializer.skip_remaining());
        ASSERT_EQ(0, deserializer.get_remaining_length());
    }
    {
        Serializer serializer;
        UnionSerializer<> union_serializer{&serializer, 2};
        union_serializer.push_back(static_cast<std::uint16_t>(0xABCD));
        ASSERT_TRUE(union_serializer.finish(4));
        const PacketBuffer golden{0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0xAB, 0xCD, 0x00, 0x00};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        UnionDeserializer<> union_deserializer{&deserializer};
        ASSERT_EQ(2U, union_deserializer.type());
        std::uint16_t value{};
        ASSERT_EQ(2, union_deserializer.pop_front(value));
        ASSERT_EQ(0xABCD, value);
        ASSERT_TRUE(union_deserializer.skip_remaining());
        ASSERT_EQ(0, deserializer.get_remaining_length());
    }
}

TEST(UnionTest, WithoutPaddingAndNull) {
    Serializer serializer;
    UnionSerializer<> unpadded{&serializer, 1};
    unpadded.push_back(static_cast<std::uint8_t>(0x7F));
    ASSERT_TRUE(unpadded.finish());
    // Type 0 is NULL: the union has no element.
    UnionSerializer<> null_union{&serializer, 0};
    ASSERT_TRUE(null_union.finish());
    const PacketBuffer golden{0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x7F,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ASSERT_EQ(golden, bytes_of(serializer));

    Deserializer deserializer{buffer_of(golden)};
    UnionDeserializer<> first{&deserializer};
    ASSERT_EQ(1U, first.type());
    std::uint8_t value{};
    ASSERT_EQ(1, first.pop_front(value));
    ASSERT_FALSE(first.has_remaining_data());
    UnionDeserializer<> second{&deserializer};
    ASSERT_TRUE(second.valid());
    ASSERT_EQ(0U, second.type());
    ASSERT_FALSE(second.has_remaining_data());
    ASSERT_EQ(0, second.pop_front(value));
}

TEST(UnionTest, ConfiguredFieldsAndOrder) {
    {
        // 16 bit length and 8 bit type.
        Serializer serializer;
        UnionSerializer<LengthFieldConfig<std::uint16_t>, std::uint8_t> union_serializer{&serializer, 3};
        union_serializer.push_back(static_cast<std::uint16_t>(0x0102));
        ASSERT_TRUE(union_serializer.finish());
        const PacketBuffer golden{0x00, 0x02, 0x03, 0x01, 0x02};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        UnionDeserializer<LengthFieldConfig<std::uint16_t>, std::uint8_t> union_deserializer{&deserializer};
        ASSERT_EQ(3, union_deserializer.type());
        std::uint16_t value{};
        ASSERT_EQ(2, union_deserializer.pop_front(value));
        ASSERT_EQ(0x0102, value);
    }
    {
        // The type field in front of the length field.
        Serializer serializer;
        UnionSerializer<LengthFieldConfig<std::uint8_t>, std::uint16_t, UnionFieldOrder::TypeFirst> union_serializer{
            &serializer, 5};
        union_serializer.push_back(static_cast<std::uint8_t>(0x42));
        ASSERT_TRUE(union_serializer.finish(2));
        const PacketBuffer golden{0x00, 0x05, 0x02, 0x42, 0x00};
        ASSERT_EQ(golden, bytes_of(serializer));

        Deserializer deserializer{buffer_of(golden)};
        UnionDeserializer<LengthFieldConfig<std::uint8_t>, std::uint16_t, UnionFieldOrder::TypeFirst>
            union_deserializer{&deserializer};
        ASSERT_EQ(5, union_deserializer.type());
        std::uint8_t value{};
        ASSERT_EQ(1, union_deserializer.pop_front(value));
        ASSERT_EQ(0x42, value);
        ASSERT_TRUE(union_deserializer.skip_remaining());
        ASSERT_EQ(0, deserializer.get_remaining_length());
    }
}

TEST(UnionTest, WithoutLengthField) {
    // feat_req_someip_583: all types have the same length, the shorter ones are padded.
    Serializer serializer;
    UnionSerializer<LengthFieldConfig<void>, std::uint8_t> union_serializer{&serializer, 1};
    union_serializer.push_back(static_cast<std::uint8_t>(0x42));
    ASSERT_TRUE(union_serializer.finish(4));
    const PacketBuffer golden{0x01, 0x42, 0x00, 0x00, 0x00, 0x99};
    PacketBuffer wire = bytes_of(serializer);
    wire.push_back(0x99);
    ASSERT_EQ(golden, wire);

    Deserializer deserializer{buffer_of(golden)};
    UnionDeserializer<LengthFieldConfig<void>, std::uint8_t> union_deserializer{&deserializer};
    ASSERT_EQ(1, union_deserializer.type());
    std::uint8_t value{};
    ASSERT_EQ(1, union_deserializer.pop_front(value));
    ASSERT_TRUE(union_deserializer.skip_remaining(4));
    ASSERT_EQ(1, deserializer.get_remaining_length());
}

TEST(UnionTest, ElementLongerThanPaddedLengthIsReported) {
    Serializer serializer;
    UnionSerializer<> union_serializer{&serializer, 1};
    union_serializer.push_back(static_cast<std::uint32_t>(1));
    ASSERT_FALSE(union_serializer.finish(2));
}

TEST(UnionTest, LengthFieldOverflowIsReported) {
    Serializer serializer;
    UnionSerializer<LengthFieldConfig<std::uint8_t>> union_serializer{&serializer, 1};
    for (int i = 0; i < 256; i++) {
        union_serializer.push_back(static_cast<std::uint8_t>(i));
    }
    ASSERT_FALSE(union_serializer.finish());
}

TEST(UnionTest, UnknownTypeIsSkipped) {
    // An element of an unknown type is skipped as a whole; the next value is still read correctly.
    const PacketBuffer input{0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x09, 0xAA, 0xBB, 0xCC, 0x12, 0x34};
    Deserializer deserializer{buffer_of(input)};
    UnionDeserializer<> union_deserializer{&deserializer};
    ASSERT_EQ(9U, union_deserializer.type());
    ASSERT_TRUE(union_deserializer.skip_remaining());
    std::uint16_t next{};
    ASSERT_EQ(2, deserializer.pop_front(next));
    ASSERT_EQ(0x1234, next);
}

TEST(UnionTest, ReadingIsLimitedByTheLength) {
    const PacketBuffer input{0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0xAA, 0xBB};
    Deserializer deserializer{buffer_of(input)};
    UnionDeserializer<> union_deserializer{&deserializer};
    std::uint16_t value{};
    ASSERT_EQ(0, union_deserializer.pop_front(value));
    std::uint8_t byte{};
    ASSERT_EQ(1, union_deserializer.pop_front(byte));
    ASSERT_EQ(0, union_deserializer.pop_front(byte));
}

TEST(UnionTest, TruncatedFieldsAreDetected) {
    const PacketBuffer cases[] = {
        {},
        {0x00, 0x00},
        {0x00, 0x00, 0x00, 0x04, 0x00, 0x00},
    };
    for (const auto& input : cases) {
        Deserializer deserializer{buffer_of(input)};
        UnionDeserializer<> union_deserializer{&deserializer};
        ASSERT_FALSE(union_deserializer.valid());
        std::uint8_t value{};
        ASSERT_EQ(0, union_deserializer.pop_front(value));
        ASSERT_FALSE(union_deserializer.skip_remaining());
    }
}

TEST(UnionTest, AnnouncedElementIsMissing) {
    const PacketBuffer input{0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0xAB};
    Deserializer deserializer{buffer_of(input)};
    UnionDeserializer<> union_deserializer{&deserializer};
    ASSERT_TRUE(union_deserializer.valid());
    std::uint8_t value{};
    ASSERT_EQ(1, union_deserializer.pop_front(value));
    ASSERT_FALSE(union_deserializer.skip_remaining());
}
