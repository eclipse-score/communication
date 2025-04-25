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
#include "score/memory/shared/atomic_indirector.h"
#include "score/memory/shared/atomic_mock.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>

namespace score::memory::shared
{
namespace
{

using ::testing::_;
using ::testing::Return;

template <template <class> class AtomicIndirectorType, typename T>
class AtomicUserTestClass
{
  public:
    using AtomicIndirector = AtomicIndirectorType<T>;

    AtomicUserTestClass(int initial_value) noexcept : atomic_{initial_value} {}

    T CallFetchAdd(int sum, std::memory_order order = std::memory_order_seq_cst)
    {
        return AtomicIndirector::fetch_add(atomic_, sum, order);
    }

    T CallFetchSub(int sum, std::memory_order order = std::memory_order_seq_cst)
    {
        return AtomicIndirector::fetch_sub(atomic_, sum, order);
    }

    bool CallCompareExchangeWeak(T& expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
        return AtomicIndirector::compare_exchange_weak(atomic_, expected, desired, order);
    }

    bool CallCompareExchangeStrong(T& expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
        return AtomicIndirector::compare_exchange_strong(atomic_, expected, desired, order);
    }

    void CallStore(int value, std::memory_order order = std::memory_order_seq_cst)
    {
        AtomicIndirector::store(atomic_, value, order);
    }

    T CallLoad(std::memory_order order = std::memory_order_seq_cst) const
    {
        return AtomicIndirector::load(atomic_, order);
    }

    constexpr bool GetIsAlwaysLockFree() const
    {
        return AtomicIndirector::is_always_lock_free;
    }

  private:
    std::atomic<T> atomic_;
};

class AtomicIndirectorFixture : public ::testing::Test
{
  protected:
    using AtomicType = int;

    AtomicIndirectorFixture() : atomic_mock_{std::make_unique<AtomicMock<AtomicType>>()}
    {
        // Setup mock object
        EXPECT_EQ(AtomicIndirectorMock<int>::GetMockObject(), nullptr);
        AtomicIndirectorMock<int>::SetMockObject(atomic_mock_.get());
        EXPECT_EQ(AtomicIndirectorMock<int>::GetMockObject(), atomic_mock_.get());
    }

    ~AtomicIndirectorFixture()
    {
        // Cleanup mock object
        AtomicIndirectorMock<int>::SetMockObject(nullptr);
        EXPECT_EQ(AtomicIndirectorMock<int>::GetMockObject(), nullptr);
    }

    const AtomicType initial_value_{10};
    const AtomicType sum_value_{10};

    std::unique_ptr<AtomicMock<AtomicType>> atomic_mock_;
};

class AtomicIndirectorRealFixture : public AtomicIndirectorFixture
{
  protected:
    AtomicUserTestClass<AtomicIndirectorReal, AtomicType> real_atomic_test_class_{initial_value_};
};

class AtomicIndirectorMockFixture : public AtomicIndirectorFixture
{
  protected:
    AtomicUserTestClass<AtomicIndirectorMock, AtomicType> mock_atomic_test_class_{initial_value_};
};

TEST_F(AtomicIndirectorRealFixture, CallingFetchAddWithRealObjectReturnsCorrectValue)
{
    std::atomic<AtomicType> atomic_value{initial_value_};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorReal
    // When calling fetch_add on an atomic via the AtomicIndirector base class
    const auto test_return_value = real_atomic_test_class_.CallFetchAdd(sum_value_);

    // Then the return value and state of the atomic value should be the same as when calling
    // std::atomic<T>::fetch_add()
    const auto atomic_return_value = atomic_value.fetch_add(sum_value_);
    EXPECT_EQ(test_return_value, atomic_return_value);
    EXPECT_EQ(real_atomic_test_class_.CallLoad(), atomic_value.load());
}

TEST_F(AtomicIndirectorRealFixture, CallingFetchSubWithRealObjectReturnsCorrectValue)
{
    std::atomic<AtomicType> atomic_value{initial_value_};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorReal
    // When calling fetch_sub on an atomic via the AtomicIndirector base class
    const auto test_return_value = real_atomic_test_class_.CallFetchSub(sum_value_);

    // Then the return value and state of the atomic value should be the same as when calling
    // std::atomic<T>::fetch_sub()
    const auto atomic_return_value = atomic_value.fetch_sub(sum_value_);
    EXPECT_EQ(test_return_value, atomic_return_value);
    EXPECT_EQ(real_atomic_test_class_.CallLoad(), atomic_value.load());
}

TEST_F(AtomicIndirectorMockFixture, CallingFetchAddWithRealObjectReturnsCorrectValue)
{
    const AtomicType mock_return_value{120};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorMock

    // Then fetch_add should be called on the mock object
    EXPECT_CALL(*atomic_mock_, fetch_add(_, _)).WillOnce(Return(mock_return_value));
    // Then load should be called on the mock object
    EXPECT_CALL(*atomic_mock_, load(_)).WillOnce(Return(initial_value_));

    // When calling fetch_add on an atomic via the AtomicIndirector base class
    const auto test_return_value = mock_atomic_test_class_.CallFetchAdd(sum_value_);

    // Then the return value should be the value specified in the EXPECT_CALL
    EXPECT_EQ(test_return_value, mock_return_value);

    // And the value of the atomic should be unchanged.
    EXPECT_EQ(mock_atomic_test_class_.CallLoad(), initial_value_);
}

TEST_F(AtomicIndirectorMockFixture, CallingFetchSubWithRealObjectReturnsCorrectValue)
{
    const AtomicType mock_return_value{120};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorMock

    // Then fetch_sub should be called on the mock object
    EXPECT_CALL(*atomic_mock_, fetch_sub(_, _)).WillOnce(Return(mock_return_value));
    // Then load should be called on the mock object
    EXPECT_CALL(*atomic_mock_, load(_)).WillOnce(Return(initial_value_));

    // When calling fetch_sub on an atomic via the AtomicIndirector base class
    const auto test_return_value = mock_atomic_test_class_.CallFetchSub(sum_value_);

    // Then the return value should be the value specified in the EXPECT_CALL
    EXPECT_EQ(test_return_value, mock_return_value);

    // And the value of the atomic should be unchanged.
    EXPECT_EQ(mock_atomic_test_class_.CallLoad(), initial_value_);
}

TEST_F(AtomicIndirectorRealFixture, CallingCompareExchangeWeakWithRealObjectReturnsCorrectValue)
{
    std::atomic<AtomicType> atomic_value{initial_value_};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorReal
    // When calling compare_exchange_weak on an atomic via the AtomicIndirector base class
    AtomicType expected{100};
    const AtomicType desired{200};
    const auto test_return_value = real_atomic_test_class_.CallCompareExchangeWeak(expected, desired);

    // Then the return value and state of the atomic value should be the same as when calling
    // std::atomic<T>::fetch_add()
    AtomicType atomic_expected{100};
    const auto atomic_return_value = atomic_value.compare_exchange_weak(atomic_expected, desired);
    EXPECT_EQ(test_return_value, atomic_return_value);
    EXPECT_EQ(real_atomic_test_class_.CallLoad(), atomic_value.load());
}

TEST_F(AtomicIndirectorMockFixture, CallingCompareExchangeWeakWithRealObjectReturnsCorrectValue)
{
    const bool mock_return_value{true};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorMock

    // Then compare_exchange_weak should be called on the mock object
    EXPECT_CALL(*atomic_mock_, compare_exchange_weak(_, _, _)).WillOnce(Return(mock_return_value));

    // Then load should be called on the mock object
    EXPECT_CALL(*atomic_mock_, load(_)).WillOnce(Return(initial_value_));

    // When calling compare_exchange_weak on an atomic via the AtomicIndirector base class
    AtomicType expected{100};
    const AtomicType desired{200};
    const auto test_return_value = mock_atomic_test_class_.CallCompareExchangeWeak(expected, desired);

    // Then the return value should be the value specified in the EXPECT_CALL
    EXPECT_EQ(test_return_value, mock_return_value);

    // And the value of the atomic should be unchanged.
    EXPECT_EQ(mock_atomic_test_class_.CallLoad(), initial_value_);
}

TEST_F(AtomicIndirectorRealFixture, CallingCompareExchangeStrongWithRealObjectReturnsCorrectValue)
{
    std::atomic<AtomicType> atomic_value{initial_value_};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorReal
    // When calling compare_exchange_strong on an atomic via the AtomicIndirector base class
    AtomicType expected{100};
    const AtomicType desired{200};
    const auto test_return_value = real_atomic_test_class_.CallCompareExchangeStrong(expected, desired);

    // Then the return value and state of the atomic value should be the same as when calling
    // std::atomic<T>::fetch_add()
    AtomicType atomic_expected{100};
    const auto atomic_return_value = atomic_value.compare_exchange_strong(atomic_expected, desired);
    EXPECT_EQ(test_return_value, atomic_return_value);
    EXPECT_EQ(real_atomic_test_class_.CallLoad(), atomic_value.load());
}

TEST_F(AtomicIndirectorMockFixture, CallingCompareExchangeStrongWithRealObjectReturnsCorrectValue)
{
    const bool mock_return_value{true};

    // Given an AtomicIndirector base class which is templated with AtomicIndirectorMock

    // Then compare_exchange_strong should be called on the mock object
    EXPECT_CALL(*atomic_mock_, compare_exchange_strong(_, _, _)).WillOnce(Return(mock_return_value));
    // Then load should be called on the mock object
    EXPECT_CALL(*atomic_mock_, load(_)).WillOnce(Return(initial_value_));

    // When calling compare_exchange_strong on an atomic via the AtomicIndirector base class
    AtomicType expected{100};
    const AtomicType desired{200};
    const auto test_return_value = mock_atomic_test_class_.CallCompareExchangeStrong(expected, desired);

    // Then the return value should be the value specified in the EXPECT_CALL
    EXPECT_EQ(test_return_value, mock_return_value);

    // And the value of the atomic should be unchanged.
    EXPECT_EQ(mock_atomic_test_class_.CallLoad(), initial_value_);
}

TEST_F(AtomicIndirectorRealFixture, CallingStoreRealObjectReturnsCorrectValue)
{
    // Given an AtomicIndirector base class which is templated with AtomicIndirectorReal

    // When calling store on an atomic via the AtomicIndirector base class
    std::atomic<AtomicType> atomic_value{initial_value_};

    real_atomic_test_class_.CallStore(sum_value_);

    atomic_value.store(sum_value_);
    // Then the return value and state of the atomic value should be the same as when calling
    // std::atomic<T>::load()
    EXPECT_EQ(real_atomic_test_class_.CallLoad(), atomic_value.load());
}

TEST_F(AtomicIndirectorMockFixture, CallingStoreMockObjectReturnsCorrectValue)
{
    // Given an AtomicIndirector base class which is templated with AtomicIndirectorMock

    const AtomicType mock_store_value{120};

    // Expecting that store will be called on the mock object
    EXPECT_CALL(*atomic_mock_, store(mock_store_value, _)).Times(1);

    // and load will be called on the mock object
    EXPECT_CALL(*atomic_mock_, load(_)).WillOnce(Return(mock_store_value));

    // When calling store on an atomic via the AtomicIndirector base class
    mock_atomic_test_class_.CallStore(mock_store_value);

    // Then the return value should be the value specified in the EXPECT_CALL
    EXPECT_EQ(mock_atomic_test_class_.CallLoad(), mock_store_value);
}

TEST_F(AtomicIndirectorRealFixture, CallingIsAlwaysLockFreeRealObjectReturnsCorrectValue)
{
    // Given an AtomicIndirector base class which is templated with AtomicIndirectorReal

    // When calling islockfree on an atomic via the AtomicIndirector base class
    std::atomic<AtomicType> atomic_value{initial_value_};

    // Then the return value should be the same as when calling std::atomic<T>::is_always_lock_free
    EXPECT_EQ(real_atomic_test_class_.GetIsAlwaysLockFree(), atomic_value.is_always_lock_free);
}

TEST_F(AtomicIndirectorRealFixture, CallingLoadRealObjectReturnsCorrectValue)
{
    // Given an AtomicIndirector base class which is templated with AtomicIndirectorReal

    std::atomic<AtomicType> atomic_value{initial_value_};

    // When calling load on an atomic via the AtomicIndirector base class
    const auto expected_load_value = atomic_value.load();

    // Then the return value should be the same as when calling std::atomic<T>::load()
    EXPECT_EQ(real_atomic_test_class_.CallLoad(), expected_load_value);
}

TEST_F(AtomicIndirectorMockFixture, CallingLoadMockObjectReturnsCorrectValue)
{
    // Given an AtomicIndirector base class which is templated with AtomicIndirectorMock

    // When load should be called on the mock object
    EXPECT_CALL(*atomic_mock_, load(_)).WillOnce(Return(initial_value_));

    // Then the return value should be the value specified in the EXPECT_CALL
    EXPECT_EQ(mock_atomic_test_class_.CallLoad(), initial_value_);
}

}  // namespace
}  // namespace score::memory::shared
