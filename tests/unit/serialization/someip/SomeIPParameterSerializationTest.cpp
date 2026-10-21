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
    const std::uint32_t kDataIdA = 0;   // NOLINT(readability-identifier-naming)
    const std::uint32_t kDataIdC = 2;   // NOLINT(readability-identifier-naming)
    const std::uint32_t kDataIdCC1 = 1; // NOLINT(readability-identifier-naming)
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
                                                                              kDataIdA};
        tlv_serializer_data_a.push_back(data.a);
        tlv_serializer_data_a.finish();
    }

    {
        TlvSerializer<LengthFieldConfig<std::uint32_t>> tlv_serializer_data_c{
            &serializer, WireTypeEnum::ComplexDataTypeFromDataDefinition, kDataIdC};
        {
            TlvSerializer<LengthFieldConfig<std::uint32_t>> tlv_serializer_data_c_c1{
                &serializer, WireTypeEnum::BaseType8Bit, kDataIdCC1};
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

    ASSERT_EQ(kDataIdA, buffer[5]); // data id = 0
    ASSERT_EQ(11, buffer[6]);       // value 11

    ASSERT_EQ(0x40, buffer[7] & 0x70); // Complext Data Type from data definition
    ASSERT_EQ(kDataIdC, buffer[8]);    // data id = 2
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
        case kDataIdA: {
            std::uint8_t d{};
            ASSERT_EQ(1, tlv_deserializer_data.pop_front(d));
            ASSERT_EQ(11, d);
            break;
        }
        case kDataIdC: {
            // The nested member reports what it reads to the member that contains it.
            TlvDeserializer<LengthFieldConfig<std::uint32_t>> tlv_deserializer_data_c{&deserializer,
                                                                                      &tlv_deserializer_data};
            TlvTag inner_tag = tlv_deserializer_data_c.consume_tag();
            ASSERT_EQ(kDataIdCC1, inner_tag.data_id_);
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
    const std::uint16_t kDataId = 0x0A23; // NOLINT(readability-identifier-naming)
    const std::uint32_t kValue = 0xCAFEBABEU;

    Serializer serializer;
    {
        TlvSerializer<LengthFieldConfig<std::uint32_t>> tlv_serializer{&serializer, WireTypeEnum::BaseType32Bit,
                                                                       kDataId};
        tlv_serializer.push_back(kValue);
        tlv_serializer.finish();
    }

    Deserializer deserializer(serializer.finish());
    TlvDeserializer<LengthFieldConfig<std::uint32_t>> tlv_deserializer{&deserializer};

    TlvTag tag = tlv_deserializer.consume_tag();
    ASSERT_EQ(kDataId, tag.data_id_);
    ASSERT_EQ(4U, tag.len_);

    std::uint32_t value{};
    tlv_deserializer.pop_front(value);
    ASSERT_EQ(kValue, value);
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
