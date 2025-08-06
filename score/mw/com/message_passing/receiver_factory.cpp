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
#include "score/mw/com/message_passing/receiver_factory.h"

#include "score/mw/com/message_passing/receiver_factory_impl.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

/// \brief Small wrapper class around a gmock of IReceiver.
/// \details gmock instances aren't copyable
class ReceiverMockWrapper final : public score::mw::com::message_passing::IReceiver
{
  public:
    explicit ReceiverMockWrapper(score::mw::com::message_passing::IReceiver* const mock)
        : IReceiver{}, wrapped_mock_{mock}
    {
    }

    void Register(const score::mw::com::message_passing::MessageId id,
                  ShortMessageReceivedCallback callback) noexcept override
    {
        return wrapped_mock_->Register(id, std::move(callback));
    }

    void Register(const score::mw::com::message_passing::MessageId id,
                  MediumMessageReceivedCallback callback) noexcept override
    {
        return wrapped_mock_->Register(id, std::move(callback));
    }

    score::cpp::expected_blank<score::os::Error> StartListening() noexcept override
    {
        return wrapped_mock_->StartListening();
    }

  private:
    score::mw::com::message_passing::IReceiver* wrapped_mock_;
};

}  // namespace

namespace score::mw::com::message_passing
{

IReceiver* ReceiverFactory::receiver_mock_{nullptr};

score::cpp::pmr::unique_ptr<IReceiver> ReceiverFactory::Create(const std::string_view identifier,
                                                        concurrency::Executor& executor,
                                                        const score::cpp::v1::span<const uid_t> allowed_user_ids,
                                                        const ReceiverConfig& receiver_config,
                                                        score::cpp::pmr::memory_resource* memory_resource)
{
    if (receiver_mock_ == nullptr)
    {
        return ReceiverFactoryImpl::Create(identifier, executor, allowed_user_ids, receiver_config, memory_resource);
    }
    else
    {
        return score::cpp::pmr::make_unique<ReceiverMockWrapper>(memory_resource, receiver_mock_);
    }
}

void ReceiverFactory::InjectReceiverMock(IReceiver* const mock)
{
    receiver_mock_ = mock;
}

}  // namespace score::mw::com::message_passing
