/*********************************************************************************
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
 *******************************************************************************/

#include "score/mw/service/provided_services_container.h"

#include "score/result/error.h"
#include "score/result/error_domain.h"
#include "score/result/result.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

namespace score::mw::service::test
{
namespace
{

class FakeErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode&) const noexcept override
    {
        return "arbitrary fake error";
    }
};

constexpr FakeErrorDomain kFakeErrorDomain{};

class FakeManagedService final : public ManagedService
{
  public:
    explicit FakeManagedService(std::uint32_t value,
                                bool fail_start = false,
                                std::uint32_t* const external_stop_count = nullptr) noexcept
        : value_{value}, fail_start_{fail_start}, external_stop_count_{external_stop_count}
    {
    }

    Result<void> Start() override
    {
        if (fail_start_)
        {
            return MakeUnexpected<void>(score::result::Error{score::result::ErrorCode{42}, kFakeErrorDomain, ""});
        }
        ++start_count_;
        return {};
    }

    // `Stop()` may run right before this instance is destroyed (e.g. via `ProvidedServicesContainer::StopAll()` or
    // its destructor), so tests that need to observe the stop count afterwards must not dereference `this` again;
    // they can instead pass in `external_stop_count`, which outlives the service instance.
    void Stop() noexcept override
    {
        ++stop_count_;
        if (external_stop_count_ != nullptr)
        {
            ++(*external_stop_count_);
        }
    }

    std::uint32_t GetValue() const noexcept
    {
        return value_;
    }
    std::uint32_t GetStartCount() const noexcept
    {
        return start_count_;
    }
    std::uint32_t GetStopCount() const noexcept
    {
        return stop_count_;
    }

  private:
    std::uint32_t value_;
    bool fail_start_;
    std::uint32_t* external_stop_count_;
    std::uint32_t start_count_{0U};
    std::uint32_t stop_count_{0U};
};

class AnotherFakeManagedService final : public ManagedService
{
  public:
    Result<void> Start() override
    {
        ++start_count_;
        return {};
    }
    void Stop() noexcept override
    {
        ++stop_count_;
    }

    std::uint32_t GetStartCount() const noexcept
    {
        return start_count_;
    }

    std::uint32_t GetStopCount() const noexcept
    {
        return stop_count_;
    }

  private:
    std::uint32_t start_count_{0U};
    std::uint32_t stop_count_{0U};
};

class FailingDefaultConstructibleManagedService final : public ManagedService
{
  public:
    Result<void> Start() override
    {
        return MakeUnexpected<void>(score::result::Error{score::result::ErrorCode{42}, kFakeErrorDomain, ""});
    }
    void Stop() noexcept override {}
};

TEST(ProvidedServicesContainer, Emplace_GivenServiceThatStartsSuccessfully_ExpectStartedAndFindable)
{
    ProvidedServicesContainer container{};

    const auto result = container.Emplace<FakeManagedService>("arbitrary_service", 42U);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(container.Size(), 1U);
    ASSERT_NE(container.Get<FakeManagedService>(), nullptr);
    EXPECT_EQ(container.Get<FakeManagedService>()->GetStartCount(), 1U);
    EXPECT_EQ(container.Get<FakeManagedService>()->GetValue(), 42U);
}

TEST(ProvidedServicesContainer, Emplace_GivenServiceThatFailsToStart_ExpectErrorAndNotStored)
{
    ProvidedServicesContainer container{};

    const auto result = container.Emplace<FakeManagedService>("arbitrary_service", 42U, true);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), 42U);
    EXPECT_EQ(container.Size(), 0U);
    EXPECT_EQ(container.Get<FakeManagedService>(), nullptr);
}

TEST(ProvidedServicesContainer,
     Emplace_GivenNoServiceIdentifierAndServiceThatStartsSuccessfully_ExpectStartedAndFindable)
{
    ProvidedServicesContainer container{};

    const auto result = container.Emplace<FakeManagedService>(42U);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(container.Size(), 1U);
    ASSERT_NE(container.Get<FakeManagedService>(), nullptr);
    EXPECT_EQ(container.Get<FakeManagedService>()->GetStartCount(), 1U);
    EXPECT_EQ(container.Get<FakeManagedService>()->GetValue(), 42U);
}

TEST(ProvidedServicesContainer, Emplace_GivenNoServiceIdentifierAndServiceThatFailsToStart_ExpectErrorAndNotStored)
{
    ProvidedServicesContainer container{};

    const auto result = container.Emplace<FakeManagedService>(42U, true);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), 42U);
    EXPECT_EQ(container.Size(), 0U);
    EXPECT_EQ(container.Get<FakeManagedService>(), nullptr);
}

TEST(ProvidedServicesContainer, Emplace_GivenNoServiceIdentifier_ExpectNotFindableByIdentifier)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>(42U).has_value());

    EXPECT_EQ(container.Get<FakeManagedService>("arbitrary_service"), nullptr);
}

TEST(ProvidedServicesContainer, Emplace_GivenNoArgumentsAndServiceThatStartsSuccessfully_ExpectStartedAndFindable)
{
    ProvidedServicesContainer container{};

    const auto result = container.Emplace<AnotherFakeManagedService>();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(container.Size(), 1U);
    ASSERT_NE(container.Get<AnotherFakeManagedService>(), nullptr);
    EXPECT_EQ(container.Get<AnotherFakeManagedService>()->GetStartCount(), 1U);
}

TEST(ProvidedServicesContainer, Emplace_GivenNoArgumentsAndServiceThatFailsToStart_ExpectErrorAndNotStored)
{
    ProvidedServicesContainer container{};

    const auto result = container.Emplace<FailingDefaultConstructibleManagedService>();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(*result.error(), 42U);
    EXPECT_EQ(container.Size(), 0U);
    EXPECT_EQ(container.Get<FailingDefaultConstructibleManagedService>(), nullptr);
}

TEST(ProvidedServicesContainer, Emplace_GivenNoArguments_ExpectNotFindableByIdentifier)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<AnotherFakeManagedService>().has_value());

    EXPECT_EQ(container.Get<AnotherFakeManagedService>("arbitrary_service"), nullptr);
}

TEST(ProvidedServicesContainer, Reserve_GivenSize_ExpectNoObservableEffectOnContents)
{
    ProvidedServicesContainer container{};

    container.Reserve(3U);

    EXPECT_EQ(container.Size(), 0U);
}

TEST(ProvidedServicesContainer, Get_GivenTypeNotEmplaced_ExpectNullptr)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    EXPECT_EQ(container.Get<AnotherFakeManagedService>(), nullptr);
}

TEST(ProvidedServicesContainer, Get_GivenConstContainer_ExpectFoundServiceAccessibleAsConst)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    const auto& const_container = container;
    const auto* const found_service = const_container.Get<FakeManagedService>();

    ASSERT_NE(found_service, nullptr);
    EXPECT_EQ(found_service->GetValue(), 42U);
}

TEST(ProvidedServicesContainer, Get_GivenMatchingIdentifier_ExpectServiceFound)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("first_service", 42U).has_value());
    ASSERT_TRUE(container.Emplace<FakeManagedService>("second_service", 43U).has_value());

    const auto* const found_service = container.Get<FakeManagedService>("second_service");

    ASSERT_NE(found_service, nullptr);
    EXPECT_EQ(found_service->GetValue(), 43U);
}

TEST(ProvidedServicesContainer, Get_GivenNonMatchingIdentifier_ExpectNullptr)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    EXPECT_EQ(container.Get<FakeManagedService>("non_matching_identifier"), nullptr);
}

TEST(ProvidedServicesContainer, Get_GivenConstContainerAndMatchingIdentifier_ExpectFoundServiceAccessibleAsConst)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    const auto& const_container = container;
    const auto* const found_service = const_container.Get<FakeManagedService>("arbitrary_service");

    ASSERT_NE(found_service, nullptr);
    EXPECT_EQ(found_service->GetValue(), 42U);
}

TEST(ProvidedServicesContainer, Has_GivenEmplacedType_ExpectTrue)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    EXPECT_TRUE(container.Has<FakeManagedService>());
    EXPECT_FALSE(container.Has<AnotherFakeManagedService>());
}

TEST(ProvidedServicesContainer, Extract_GivenEmplacedService_ExpectRemovedFromContainerAndOwnershipTransferred)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    auto extracted_service = container.Extract<FakeManagedService>();

    ASSERT_NE(extracted_service, nullptr);
    EXPECT_EQ(extracted_service->GetValue(), 42U);
    EXPECT_EQ(container.Size(), 0U);
    EXPECT_EQ(container.Get<FakeManagedService>(), nullptr);
}

TEST(ProvidedServicesContainer, Extract_GivenTypeNotEmplaced_ExpectNullptr)
{
    ProvidedServicesContainer container{};

    auto extracted_service = container.Extract<FakeManagedService>();

    EXPECT_EQ(extracted_service, nullptr);
}

TEST(ProvidedServicesContainer, Extract_GivenExtractedService_ExpectStopNotCalledByContainer)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    auto extracted_service = container.Extract<FakeManagedService>();

    ASSERT_NE(extracted_service, nullptr);
    EXPECT_EQ(extracted_service->GetStopCount(), 0U);
}

TEST(ProvidedServicesContainer, Extract_GivenMatchingIdentifier_ExpectOnlyMatchingServiceExtracted)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("first_service", 42U).has_value());
    ASSERT_TRUE(container.Emplace<FakeManagedService>("second_service", 43U).has_value());

    auto extracted_service = container.Extract<FakeManagedService>("second_service");

    ASSERT_NE(extracted_service, nullptr);
    EXPECT_EQ(extracted_service->GetValue(), 43U);
    EXPECT_EQ(container.Size(), 1U);
    ASSERT_NE(container.Get<FakeManagedService>("first_service"), nullptr);
    EXPECT_EQ(container.Get<FakeManagedService>("second_service"), nullptr);
}

TEST(ProvidedServicesContainer, Extract_GivenNonMatchingIdentifier_ExpectNullptrAndNoRemoval)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    auto extracted_service = container.Extract<FakeManagedService>("non_matching_identifier");

    EXPECT_EQ(extracted_service, nullptr);
    EXPECT_EQ(container.Size(), 1U);
}

TEST(ProvidedServicesContainer, StopAll_GivenEmplacedServices_ExpectAllStoppedAndContainerEmptied)
{
    // `StopAll()` destroys the emplaced services, so the stop counts are observed via counters owned by the test
    // instead of dereferencing the (now-dangling) service pointers afterwards.
    std::uint32_t first_stop_count{0U};
    std::uint32_t second_stop_count{0U};

    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("first_service", 42U, false, &first_stop_count).has_value());
    ASSERT_TRUE(container.Emplace<FakeManagedService>("second_service", 43U, false, &second_stop_count).has_value());

    container.StopAll();

    EXPECT_EQ(first_stop_count, 1U);
    EXPECT_EQ(second_stop_count, 1U);
    EXPECT_EQ(container.Size(), 0U);
}

TEST(ProvidedServicesContainer, Destructor_GivenEmplacedService_ExpectStopCalled)
{
    // The container's destructor destroys the emplaced service, so the stop count is observed via a counter owned
    // by the test instead of dereferencing the (now-dangling) service pointer afterwards.
    std::uint32_t stop_count{0U};

    {
        ProvidedServicesContainer container{};
        ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U, false, &stop_count).has_value());
        EXPECT_EQ(stop_count, 0U);
    }

    EXPECT_EQ(stop_count, 1U);
}

TEST(ProvidedServicesContainer, MoveConstruct_GivenContainerWithService_ExpectServiceMovedAndFindable)
{
    ProvidedServicesContainer container{};
    ASSERT_TRUE(container.Emplace<FakeManagedService>("arbitrary_service", 42U).has_value());

    ProvidedServicesContainer moved_container{std::move(container)};

    ASSERT_NE(moved_container.Get<FakeManagedService>(), nullptr);
    EXPECT_EQ(moved_container.Get<FakeManagedService>()->GetValue(), 42U);
    EXPECT_EQ(moved_container.Size(), 1U);
}

}  // namespace
}  // namespace score::mw::service::test
