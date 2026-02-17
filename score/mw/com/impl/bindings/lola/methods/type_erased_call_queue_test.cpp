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
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"

#include "score/memory/data_type_size_info.h"

#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/assert_support.hpp>

#include <score/utility.hpp>
#include <gtest/gtest.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

const std::size_t kQueueSize{9U};

constexpr memory::DataTypeSizeInfo kValidInArgTypeErasedInfo{104U, 8U};
const auto kExpectedInArgAllocationSize = kValidInArgTypeErasedInfo.Size() * kQueueSize;

constexpr memory::DataTypeSizeInfo kValidReturnTypeErasedInfo{64U, 16U};
const auto kExpectedReturnValueAllocationSize = kValidReturnTypeErasedInfo.Size() * kQueueSize;

const std::size_t kExpectedPaddingBeweenInArgsAndReturnValue{8U};

class TypeErasedCallQueueFixture : public ::testing::Test
{
  public:
    TypeErasedCallQueueFixture()
    {
        memory::shared::MemoryResourceProxy::EnableBoundsChecking(false);
    }

    TypeErasedCallQueueFixture& WithAnInArgTypeInfo()
    {
        type_erased_element_info_.in_arg_type_info = kValidInArgTypeErasedInfo;
        return *this;
    }

    TypeErasedCallQueueFixture& WithAReturnTypeInfo()
    {
        type_erased_element_info_.return_type_info = kValidReturnTypeErasedInfo;
        return *this;
    }

    TypeErasedCallQueueFixture& GivenATypeErasedCallQueue()
    {
        unit_ = std::make_unique<TypeErasedCallQueue>(*memory_resource_proxy_, type_erased_element_info_);
        return *this;
    }

    memory::shared::test::MyBoundedMemoryResource fake_memory_resource_{2000U};
    const memory::shared::MemoryResourceProxy* memory_resource_proxy_{fake_memory_resource_.getMemoryResourceProxy()};

    TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info_{{}, {}, kQueueSize};

    std::unique_ptr<TypeErasedCallQueue> unit_{nullptr};
};

using TypeErasedCallQueueAllocationFixture = TypeErasedCallQueueFixture;
TEST_F(TypeErasedCallQueueAllocationFixture, DoesNotAllocatesIfNoInArgTypeOrReturnTypeInfoProvided)
{
    // When constructing a TypeErasedCallQueue with empty InArg and Return TypeInfos
    GivenATypeErasedCallQueue();

    // Then no memory should have been allocated
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), 0U);
}

TEST_F(TypeErasedCallQueueAllocationFixture, AllocatesInArgOnConstructionIfInArgTypeInfoProvided)
{
    // When constructing a TypeErasedCallQueue with only an InArg TypeInfo
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // Then memory should have been allocated for all the in args in the queue taking into account element padding
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), kExpectedInArgAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, AllocatesReturnOnConstructionIfReturnTypeInfoProvided)
{
    // When constructing a TypeErasedCallQueue with only an Return TypeInfo
    WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // Then memory should have been allocated for all the return values in the queue taking into account element padding
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), kExpectedReturnValueAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, AllocatesReturnAndInArgsOnConstructionIfReturnAndInArgTypeInfosProvided)
{
    // When constructing a TypeErasedCallQueue with both InArg and Return TypeInfos
    WithAnInArgTypeInfo().WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // Then memory should have been allocated for all the in args and return values in the queue
    const auto total_allocation_size =
        kExpectedInArgAllocationSize + kExpectedPaddingBeweenInArgsAndReturnValue + kExpectedReturnValueAllocationSize;
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), total_allocation_size);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DoesNotDeallocateIfNoTypeInfosProvided)
{
    GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the no memory should be deallocated
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), 0U);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DeallocatesInArgsOnDestructionIfInArgTypeInfoProvided)
{
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the memory that was allocated for the InArgs should have been deallocated
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), kExpectedInArgAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DeallocatesReturnOnDestructionIfReturnTypeInfoProvided)
{
    WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the memory that was allocated for the Return should have been deallocated
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), kExpectedReturnValueAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DeallocatesInArgsAndReturnOnDestructionIfInArgAndReturnTypeInfosProvided)
{
    WithAnInArgTypeInfo().WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the memory that was allocated for the InArgs and Return should have been deallocated
    const auto expected_deallocation_size = kExpectedInArgAllocationSize + kExpectedReturnValueAllocationSize;
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), expected_deallocation_size);
}

TEST_F(TypeErasedCallQueueFixture, GetInArgValuesQueueStoragePointsToCorrectPositionInQueueWithOnlyInArgs)
{
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // Given a valid queue storage and TypeErasedElementInfo for InArgs
    const auto in_args_queue_storage = unit_->GetInArgValuesQueueStorage();
    ASSERT_TRUE(in_args_queue_storage.has_value());
    const auto in_args_type_erased_info = unit_->GetTypeErasedElementInfo();

    auto* const queue_start_address = fake_memory_resource_.getUsableBaseAddress();
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        // When calling GetInArgValuesElementStorage with the index of every element in the queue
        auto in_args_queue_position_storage =
            GetInArgValuesElementStorage(i, in_args_queue_storage.value(), in_args_type_erased_info);

        // Then the returned storage should have the correct address and size
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(queue_start_address, i * kValidInArgTypeErasedInfo.Size());
        EXPECT_EQ(in_args_queue_position_storage.data(), expected_element_address);
        EXPECT_EQ(in_args_queue_position_storage.size(), kValidInArgTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetInArgValuesQueueStorageUsesInArgsSizeTimesQueueSize)
{
    // Given a TypeErasedElementInfo extracted from a TypeErasedCallQueue with a queue size larger than 1
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgValuesQueueStorage()
    const auto in_args_queue_storage = unit_->GetInArgValuesQueueStorage();
    ASSERT_TRUE(in_args_queue_storage.has_value());

    const auto in_args_type_erased_info = unit_->GetTypeErasedElementInfo();

    // Then its size fits the InArgs kQueueSize-times
    ASSERT_EQ(in_args_queue_storage->size(), in_args_type_erased_info.in_arg_type_info->Size() * kQueueSize);
}

TEST_F(TypeErasedCallQueueFixture, GetReturnValueQueueStorageUsesReturnSizeTimesQueueSize)
{
    // Given a TypeErasedElementInfo extracted from a TypeErasedCallQueue with a queue size larger than 1
    WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetReturnValueQueueStorage()
    const auto return_queue_storage = unit_->GetReturnValueQueueStorage();
    ASSERT_TRUE(return_queue_storage.has_value());

    const auto in_args_type_erased_info = unit_->GetTypeErasedElementInfo();

    // Then its size fits the Return values kQueueSize-times
    ASSERT_EQ(return_queue_storage->size(), in_args_type_erased_info.return_type_info->Size() * kQueueSize);
}

TEST_F(TypeErasedCallQueueFixture, GetInArgValuesQueueStoragePointsToCorrectPositionInQueueWithInArgsAndReturnTypeInfos)
{
    WithAnInArgTypeInfo().WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // Given a valid queue storage and TypeErasedElementInfo for InArgs
    const auto in_args_queue_storage = unit_->GetInArgValuesQueueStorage();
    ASSERT_TRUE(in_args_queue_storage.has_value());
    const auto in_args_type_erased_info = unit_->GetTypeErasedElementInfo();

    auto* const queue_start_address = fake_memory_resource_.getUsableBaseAddress();
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        // When calling GetInArgValuesElementStorage with the index of every element in the queue
        auto in_args_queue_position_storage =
            GetInArgValuesElementStorage(i, in_args_queue_storage.value(), in_args_type_erased_info);

        // Then the returned storage should have the correct address and size
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(queue_start_address, i * kValidInArgTypeErasedInfo.Size());
        EXPECT_EQ(in_args_queue_position_storage.data(), expected_element_address);
        EXPECT_EQ(in_args_queue_position_storage.size(), kValidInArgTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetReturnValueQueueStoragePointsToCorrectPositionInQueue)
{
    WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // Given a valid queue storage and TypeErasedElementInfo for Return value
    const auto return_value_queue_storage = unit_->GetReturnValueQueueStorage();
    ASSERT_TRUE(return_value_queue_storage.has_value());
    const auto return_value_type_erased_info = unit_->GetTypeErasedElementInfo();

    auto* const queue_start_address = fake_memory_resource_.getUsableBaseAddress();
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        // When calling GetReturnValueElementStorage with the index of every element in the queue
        auto return_value_queue_position_storage =
            GetReturnValueElementStorage(i, return_value_queue_storage.value(), return_value_type_erased_info);

        // Then the returned storage should have the correct address and size
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(queue_start_address, i * kValidReturnTypeErasedInfo.Size());
        EXPECT_EQ(return_value_queue_position_storage.data(), expected_element_address);
        EXPECT_EQ(return_value_queue_position_storage.size(), kValidReturnTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetReturnValueQueueStoragePointsToCorrectPositionInQueueWithInArgsAndReturnTypeInfos)
{
    WithAnInArgTypeInfo().WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // Given a valid queue storage and TypeErasedElementInfo for Return value
    const auto return_value_queue_storage = unit_->GetReturnValueQueueStorage();
    ASSERT_TRUE(return_value_queue_storage.has_value());
    const auto return_value_type_erased_info = unit_->GetTypeErasedElementInfo();

    const auto expected_in_arg_allocation_size_plus_padding =
        kValidInArgTypeErasedInfo.Size() * kQueueSize + kExpectedPaddingBeweenInArgsAndReturnValue;
    auto* const return_queue_start_address = memory::shared::AddOffsetToPointer(
        fake_memory_resource_.getUsableBaseAddress(), expected_in_arg_allocation_size_plus_padding);
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        // When calling GetReturnValueElementStorage with the index of every element in the queue
        auto return_value_queue_position_storage =
            GetReturnValueElementStorage(i, return_value_queue_storage.value(), return_value_type_erased_info);

        // Then the returned storage should have the correct address and size
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(return_queue_start_address, i * kValidReturnTypeErasedInfo.Size());
        EXPECT_EQ(return_value_queue_position_storage.data(), expected_element_address);
        EXPECT_EQ(return_value_queue_position_storage.size(), kValidReturnTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetInArgValuesQueueStorageWithoutProvidingInArgTypeInfoReturnsEmptyOptional)
{
    WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgValuesQueueStorage
    auto in_args_queue_storage = unit_->GetInArgValuesQueueStorage();

    // Then an empty optional should be returned
    EXPECT_FALSE(in_args_queue_storage.has_value());
}

TEST_F(TypeErasedCallQueueFixture, GetReturnValueQueueStorageWithoutProvidingReturnTypeInfoReturnsEmptyOptional)
{
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetReturnValueQueueStorage
    auto return_value_queue_storage = unit_->GetReturnValueQueueStorage();

    // Then an empty optional should be returned
    EXPECT_FALSE(return_value_queue_storage.has_value());
}

TEST_F(TypeErasedCallQueueFixture, GetInArgValuesElementStorageWithoutProvidingInArgTypeInfoTerminates)
{
    WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    std::array<std::byte, 100> valid_storage{};

    // When calling GetInArgValuesQueueStorage with a queue_position and storage but a TypeErasedElementInfo which
    // contains an empty optional for InArg type info
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        score::cpp::ignore = GetInArgValuesElementStorage(
            0U, {valid_storage.data(), valid_storage.size()}, unit_->GetTypeErasedElementInfo()));
}

TEST_F(TypeErasedCallQueueFixture, GetReturnValueStorageWithoutProvidingReturnTypeInfoTerminates)
{
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    std::array<std::byte, 100> valid_storage{};

    // When calling GetReturnValueElementStorage with a queue_position and storage but a TypeErasedElementInfo which
    // contains an empty optional for Return value type info
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(
        score::cpp::ignore = GetReturnValueElementStorage(
            0U, {valid_storage.data(), valid_storage.size()}, unit_->GetTypeErasedElementInfo()));
}

TEST_F(TypeErasedCallQueueFixture, GetInArgValuesQueueStorageWithOutOfRangePositionTerminates)
{
    WithAnInArgTypeInfo().WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // Given a valid queue storage and TypeErasedElementInfo for InArgs
    const auto in_args_queue_storage = unit_->GetInArgValuesQueueStorage();
    ASSERT_TRUE(in_args_queue_storage.has_value());
    const auto in_args_type_erased_info = unit_->GetTypeErasedElementInfo();

    // When calling GetInArgValuesElementStorage with an index which is out of the queue range
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = GetInArgValuesElementStorage(
                                     kQueueSize, in_args_queue_storage.value(), in_args_type_erased_info));
}

TEST_F(TypeErasedCallQueueFixture, GetReturnValueQueueStorageWithOutOfRangePositionTerminates)
{
    WithAnInArgTypeInfo().WithAReturnTypeInfo().GivenATypeErasedCallQueue();

    // Given a valid queue storage and TypeErasedElementInfo for Return value
    const auto return_value_queue_storage = unit_->GetReturnValueQueueStorage();
    ASSERT_TRUE(return_value_queue_storage.has_value());
    const auto return_value_type_erased_info = unit_->GetTypeErasedElementInfo();

    // When calling GetReturnValueElementStorage with an index which is out of the queue range
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = GetReturnValueElementStorage(
                                     kQueueSize, return_value_queue_storage.value(), return_value_type_erased_info));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
