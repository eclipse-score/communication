/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/util/type_erased_storage.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{

namespace
{
std::array<std::byte, 1024> memory{};

/// \brief sequence of arguments of given types. The order is chosen this way as it forces some padding!
std::tuple<std::uint8_t, std::uint64_t, std::uint32_t, std::uint64_t> test_arguments_1{1U, 64U, 32U, 64U};

/// \brief This is the structure datatype into which the test_arguments_1 shall be laid out by our type-erased storage
///        I.e. the binary representation we do on-the-fly of the given list of typed values in test_arguments_1, shall
///        exactly reflect this struct representation (created by the compiler).
struct equivalent_struct_test_arguments_1
{
    std::uint8_t a;
    std::uint64_t b;
    std::uint32_t c;
    std::uint64_t d;
};

/// \brief sequence of arguments of given types. These argument sequence requires to add a "final padding" to calculate
/// the size correctly! I.e. to explicitly assure, that the size always has to be a multiple of its alignment!
/// \see https://en.cppreference.com/w/cpp/language/sizeof.html
std::tuple<std::uint8_t, std::uint64_t, std::uint32_t, std::uint64_t, std::uint8_t> test_arguments_2{1U,
                                                                                                     64U,
                                                                                                     32U,
                                                                                                     64U,
                                                                                                     1U};

/// \brief This is the structure datatype into which the test_arguments_2 shall be laid out by our type-erased storage
///        I.e. the binary representation we do on-the-fly of the given list of typed values in test_arguments_2, shall
///        exactly reflect this struct representation (created by the compiler).
struct equivalent_struct_test_arguments_2
{
    std::uint8_t a;
    std::uint64_t b;
    std::uint32_t c;
    std::uint64_t d;
    std::uint8_t e;
};

struct InnerStructType
{
    std::array<std::uint8_t, 9> x;
    std::uint64_t y;
    bool z;
};

bool operator==(const InnerStructType& lhs, const InnerStructType& rhs) noexcept
{
    return ((lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.z == rhs.z));
}

std::ostream& operator<<(std::ostream& ostream_out, const InnerStructType& inner_struct) noexcept
{
    ostream_out << "inner_struct.x size " << inner_struct.x.size() << "inner_struct.y " << inner_struct.y
                << "inner_struct.z " << inner_struct.z;
    return ostream_out;
}

/// \brief sequence of arguments of given types. These are test args, which also contain an aggregate (struct) besides
/// scalars.
std::tuple<std::uint8_t, std::uint64_t, std::uint32_t, InnerStructType, std::uint8_t> test_arguments_3{
    1U,
    64U,
    32U,
    {{0, 1, 2, 3, 4, 5, 6, 7, 8}, 64U, true},
    1U};

/// \brief This is the structure datatype into which the test_arguments_3 shall be laid out by our type-erased storage
///        I.e. the binary representation we do on-the-fly of the given list of typed values in test_arguments_3, shall
///        exactly reflect this struct representation (created by the compiler).
struct equivalent_struct_test_arguments_3
{
    std::uint8_t a;
    std::uint64_t b;
    std::uint32_t c;
    InnerStructType d;
    std::uint8_t e;
};

/// \brief Wrapper for CreateTypeErasedDataTypeInfoFromTypes, which is needed, when
/// CreateTypeErasedDataTypeInfoFromTypes gets called by std::apply.
/// \details Within our std::apply usage, we need to wrap our CreateTypeErasedDataTypeInfoFromXXX function templates
/// into lambda, since directly using our function template does not work (Error: "can't deduce the function type"), see
/// https://en.cppreference.com/w/cpp/utility/apply.html -> Code example.
/// But since C++17 doesn't support template-lambdas, we use a regular lambda, which calls this wrapper, where we can
/// deduce the type-args for the right call to CreateTypeErasedDataTypeInfoFromTypes. This is just needed for
/// CreateTypeErasedDataTypeInfoFromTypes, because it has no call arguments from which to deduce the types!
template <typename... Args>
TypeErasedDataTypeInfo CreateTypeErasedDataTypeInfoFromTypesWrapper(Args...)
{
    return CreateTypeErasedDataTypeInfoFromTypes<Args...>();
}

template <typename... Args>
std::tuple<typename std::add_pointer<Args>::type...> DeserializeFromTypesWrapper(MemoryBufferAccessor& buffer, Args...)
{
    return Deserialize<Args...>(buffer);
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromValues_1)
{
    // given a TypeErasedDataTypeInfo calculated from test_arguments_1
    auto result = std::apply(
        [](auto&&... args) {
            return CreateTypeErasedDataTypeInfoFromValues(args...);
        },
        test_arguments_1);

    // expect, that it has exactly the same size as its equivalent struct representation.
    EXPECT_EQ(result.size, sizeof(equivalent_struct_test_arguments_1));
    // expect, that it has exactly the same alignment as its equivalent struct representation.
    EXPECT_EQ(result.alignment, alignof(equivalent_struct_test_arguments_1));
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromValues_2)
{
    // given a TypeErasedDataTypeInfo calculated from test_arguments_2
    auto result = std::apply(
        [](auto&&... args) {
            return CreateTypeErasedDataTypeInfoFromValues(args...);
        },
        test_arguments_2);

    // expect, that it has exactly the same size as its equivalent struct representation.
    EXPECT_EQ(result.size, sizeof(equivalent_struct_test_arguments_2));
    // expect, that it has exactly the same alignment as its equivalent struct representation.
    EXPECT_EQ(result.alignment, alignof(equivalent_struct_test_arguments_2));
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromValues_3)
{
    // given a TypeErasedDataTypeInfo calculated from test_arguments_3
    auto result = std::apply(
        [](auto&&... args) {
            return CreateTypeErasedDataTypeInfoFromValues(args...);
        },
        test_arguments_3);

    // expect, that it has exactly the same size as its equivalent struct representation.
    EXPECT_EQ(result.size, sizeof(equivalent_struct_test_arguments_3));
    // expect, that it has exactly the same alignment as its equivalent struct representation.
    EXPECT_EQ(result.alignment, alignof(equivalent_struct_test_arguments_3));
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromTypes_1)
{
    // given a TypeErasedDataTypeInfo calculated from test_arguments_1
    auto result = std::apply(
        [](auto&&... args) {
            return CreateTypeErasedDataTypeInfoFromTypesWrapper(args...);
        },
        test_arguments_1);

    // expect, that it has exactly the same size as its equivalent struct representation.
    EXPECT_EQ(result.size, sizeof(equivalent_struct_test_arguments_1));
    // expect, that it has exactly the same alignment as its equivalent struct representation.
    EXPECT_EQ(result.alignment, alignof(equivalent_struct_test_arguments_1));
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromType_2)
{
    // given a TypeErasedDataTypeInfo calculated from test_arguments_2
    auto result = std::apply(
        [](auto&&... args) {
            return CreateTypeErasedDataTypeInfoFromTypesWrapper(args...);
        },
        test_arguments_2);

    // expect, that it has exactly the same size as its equivalent struct representation.
    EXPECT_EQ(result.size, sizeof(equivalent_struct_test_arguments_2));
    // expect, that it has exactly the same alignment as its equivalent struct representation.
    EXPECT_EQ(result.alignment, alignof(equivalent_struct_test_arguments_2));
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromTypes_3)
{
    // given a TypeErasedDataTypeInfo calculated from test_arguments_2
    auto result = std::apply(
        [](auto&&... args) {
            return CreateTypeErasedDataTypeInfoFromTypesWrapper(args...);
        },
        test_arguments_3);

    // expect, that it has exactly the same size as its equivalent struct representation.
    EXPECT_EQ(result.size, sizeof(equivalent_struct_test_arguments_3));
    // expect, that it has exactly the same alignment as its equivalent struct representation.
    EXPECT_EQ(result.alignment, alignof(equivalent_struct_test_arguments_3));
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromTypes_constexpr)
{
    // Expect, that instantiation of a std::array specifying its size depending on the outcome of a call to
    // CreateTypeErasedDataTypeInfoFromTypes() compiles.
    std::array<std::uint8_t, CreateTypeErasedDataTypeInfoFromTypes<int, char, int>().size> dummy_array{};
    EXPECT_GT(dummy_array.size(), 0U);
}

TEST(TypeErasedStorageTest, CreateTypeErasedDataTypeInfoFromValues_constexpr)
{
    // Expect, that instantiation of a std::array specifying its size depending on the outcome of a call to
    // CreateTypeErasedDataTypeInfoFromValues() compiles.
    const int i{0}, j{0}, k{0};
    std::array<std::uint8_t, CreateTypeErasedDataTypeInfoFromValues(i, j, k).size> dummy_array{};
    EXPECT_GT(dummy_array.size(), 0U);
}

/// \brief Test helper to compare a pointer To arg with an arg value.
/// \tparam Ptr
/// \tparam Val
/// \param ptr
/// \param value
/// \param compare_result
template <typename Ptr, typename Val>
void CompareResultPtrToArgumentVal(const Ptr& ptr, const Val& value, bool& compare_result)
{
    std::cout << "comparing " << typeid(Ptr).name() << " with " << typeid(Val).name() << std::endl;
    bool equal = (*ptr == value);
    std::cout << "*ptr: " << *ptr << "value: " << value << " is equal: " << equal << std::endl;

    compare_result &= (*ptr == value);
}

/// \brief Test helper to compare a tuple of argument-pointers as returned from #Deserialize<Args...>() template func
/// with the corresponding argument values as stored within our test_arguments_xxx variables.
/// \tparam PtrTupleT Type of Tuple of argument pointers
/// \tparam ValueTupleT Type of Tuple of argument values
/// \tparam Is index sequence generated for tuple-size (both tuples are expected to have the same size)
/// \param ptr_tp Tuple of argument pointers
/// \param value_tp Tuple of argument values
/// \return true, if all dereferenced pointers from the 1st tuple compare equal to the corresponding value in the 2nd
/// tuple.
template <typename PtrTupleT, typename ValueTupleT, std::size_t... Is>
bool CompareResultPtrTOArgumentValTuples(const PtrTupleT& ptr_tp,
                                         const ValueTupleT& value_tp,
                                         std::index_sequence<Is...>)
{
    bool compare_result{true};
    (CompareResultPtrToArgumentVal(std::get<Is>(ptr_tp), std::get<Is>(value_tp), compare_result), ...);
    return compare_result;
}

TEST(TypeErasedStorageTest, SerializeAndDeserialize_1)
{
    MemoryBufferAccessor buffer{&memory[0], sizeof(memory)};

    // Given  serialized test_arguments_1
    std::apply(
        [&buffer](auto&&... args) {
            SerializeArgs(buffer, args...);
        },
        test_arguments_1);

    // expect, that some buffer space has been used for serialization ...
    EXPECT_GT(buffer.offset, 0);

    // When deserializing the buffer again
    buffer.offset = 0;
    auto deserialize_result = std::apply(
        [&buffer](auto&&... args) {
            return DeserializeFromTypesWrapper(buffer, args...);
        },
        test_arguments_1);

    // expect, that the size of the tuple containing the argument pointers matches the tuple size of the argument
    // values.
    EXPECT_EQ(std::tuple_size_v<decltype(test_arguments_1)>, std::tuple_size_v<decltype(deserialize_result)>);

    // and expect, that each dereferenced pointer value matches the argument value.
    EXPECT_TRUE(CompareResultPtrTOArgumentValTuples(
        deserialize_result,
        test_arguments_1,
        std::make_index_sequence<std::tuple_size<decltype(test_arguments_1)>::value>{}));
}

TEST(TypeErasedStorageTest, SerializeAndDeserialize_2)
{
    MemoryBufferAccessor buffer{&memory[0], sizeof(memory)};

    // Given  serialized test_arguments_3 (containing aggregate type)
    std::apply(
        [&buffer](auto&&... args) {
            SerializeArgs(buffer, args...);
        },
        test_arguments_3);

    // expect, that some buffer space has been used for serialization ...
    EXPECT_GT(buffer.offset, 0);

    // When deserializing the buffer again
    buffer.offset = 0;
    auto deserialize_result = std::apply(
        [&buffer](auto&&... args) {
            return DeserializeFromTypesWrapper(buffer, args...);
        },
        test_arguments_3);

    // expect, that the size of the tuple containing the argument pointers matches the tuple size of the argument
    // values.
    EXPECT_EQ(std::tuple_size_v<decltype(test_arguments_3)>, std::tuple_size_v<decltype(deserialize_result)>);

    // and expect, that each dereferenced pointer value matches the argument value.
    EXPECT_TRUE(CompareResultPtrTOArgumentValTuples(
        deserialize_result,
        test_arguments_3,
        std::make_index_sequence<std::tuple_size<decltype(test_arguments_1)>::value>{}));
}

/// \brief Rough code snippet, how an impl::ProxyMethod is using TypeErasedDataTypeInfo.
/// It creates (compile time static type_erased_in_args_ from the Inarg template args.
/// This type_erased_in_args_ is then the only info, which gets handed down to the LoLa binding layer!
template <typename... Args>
class DummyProxyMethod
{
  public:
    DummyProxyMethod() = default;
    static TypeErasedDataTypeInfo GetTypeErasedDataTypeInfo()
    {
        return type_erased_in_args_;
    }

  private:
    constexpr static TypeErasedDataTypeInfo type_erased_in_args_{CreateTypeErasedDataTypeInfoFromTypes<Args...>()};
};

TEST(TypeErasedStorageTest, SimulateProxyMethodUseCase)
{
    DummyProxyMethod<std::uint8_t, std::uint64_t, char> proxy_method{};
    auto proxy_method_type_erased_data_type_info = proxy_method.GetTypeErasedDataTypeInfo();
    auto equivalent_type_erased_data_type_info =
        CreateTypeErasedDataTypeInfoFromTypes<std::uint8_t, std::uint64_t, char>();
    EXPECT_EQ(proxy_method_type_erased_data_type_info, equivalent_type_erased_data_type_info);
}

}  // namespace

}  // namespace score::mw::com::impl
