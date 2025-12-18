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
#include "score/mw/com/message_passing/sender_factory.h"

#include "score/mw/com/message_passing/sender_factory_impl.h"

#include <memory>
#include <utility>

namespace
{

/// \brief Small wrapper class around a gmock of ISender.
/// \details gmock instances aren't copyable
class SenderMockWrapper : public score::mw::com::message_passing::ISender
{
  public:
    // coverity[autosar_cpp14_m0_1_9_violation] false-positive: constructor; not dead code
    // coverity[autosar_cpp14_m0_1_3_violation] false-positive: constructor; not a variable
    // coverity[autosar_cpp14_a0_1_3_violation] false-positive: used in tests
    explicit SenderMockWrapper(score::mw::com::message_passing::ISender* const mock) : ISender{}, wrapped_mock_{mock} {}

    score::cpp::expected_blank<score::os::Error> Send(
        const score::mw::com::message_passing::ShortMessage& message) noexcept override
    {
        return wrapped_mock_->Send(message);
    }

    score::cpp::expected_blank<score::os::Error> Send(
        const score::mw::com::message_passing::MediumMessage& message) noexcept override
    {
        return wrapped_mock_->Send(message);
    }

    bool HasNonBlockingGuarantee() const noexcept override
    {
        return wrapped_mock_->HasNonBlockingGuarantee();
    }

  private:
    score::mw::com::message_passing::ISender* wrapped_mock_;
};

// Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
// constant-initialized.".
// Not safety related code; only accessible during testing when `sender_mock_` is set up.
// coverity[autosar_cpp14_a3_3_2_violation]
// coverity[autosar_cpp14_m0_1_9_violation] false-positive: used in SenderFactory::Create(); not dead code
// coverity[autosar_cpp14_m0_1_3_violation] false-positive: see above
score::cpp::callback<void(const score::cpp::stop_token&)> callback_{};

}  // namespace

namespace score::mw::com::message_passing
{
// coverity[autosar_cpp14_a0_1_1_violation] false-positive: is used in tests
ISender* SenderFactory::sender_mock_{nullptr};

// coverity[autosar_cpp14_a2_10_4_violation] false-positive: name is not reused; system-specific implementation
score::cpp::pmr::unique_ptr<ISender> SenderFactory::Create(const std::string_view identifier,
                                                    const score::cpp::stop_token& token,
                                                    const SenderConfig& sender_config,
                                                    LoggingCallback logging_callback,
                                                    score::cpp::pmr::memory_resource* const memory_resource)
{
    if (sender_mock_ == nullptr)
    {
        return SenderFactoryImpl::Create(
            identifier, token, sender_config, std::move(logging_callback), memory_resource);
    }
    else
    {
        callback_(token);
        return score::cpp::pmr::make_unique<SenderMockWrapper>(memory_resource, sender_mock_);
    }
}

void SenderFactory::InjectSenderMock(ISender* const mock, score::cpp::callback<void(const score::cpp::stop_token&)>&& callback)
{
    sender_mock_ = mock;
    callback_ = std::move(callback);
}

}  // namespace score::mw::com::message_passing
