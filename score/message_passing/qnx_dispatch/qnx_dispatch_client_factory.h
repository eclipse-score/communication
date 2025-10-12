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
#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_CLIENT_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_CLIENT_FACTORY_H

#include "score/message_passing/i_client_factory.h"

namespace score
{
namespace message_passing
{

// Suppress "AUTOSAR C++14 M3-2-3" rule finding. This rule states: "A type, object or function that is used in multiple
// translation units shall be declared in one and only one file.".
// This is a forward declaration
// coverity[autosar_cpp14_m3_2_3_violation]
class QnxDispatchEngine;

class QnxDispatchClientFactory final : public IClientFactory
{
  public:
    explicit QnxDispatchClientFactory(
        score::cpp::pmr::memory_resource* const resource = score::cpp::pmr::get_default_resource()) noexcept;
    explicit QnxDispatchClientFactory(const std::shared_ptr<QnxDispatchEngine> engine) noexcept;
    ~QnxDispatchClientFactory() noexcept;

    QnxDispatchClientFactory(const QnxDispatchClientFactory&) = delete;
    QnxDispatchClientFactory(QnxDispatchClientFactory&&) = delete;
    QnxDispatchClientFactory& operator=(const QnxDispatchClientFactory&) = delete;
    QnxDispatchClientFactory& operator=(QnxDispatchClientFactory&&) = delete;

    score::cpp::pmr::unique_ptr<IClientConnection> Create(const ServiceProtocolConfig& protocol_config,
                                                   const ClientConfig& client_config) noexcept override;

    std::shared_ptr<QnxDispatchEngine> GetEngine() const noexcept
    {
        return engine_;
    }

  private:
    const std::shared_ptr<QnxDispatchEngine> engine_;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_CLIENT_FACTORY_H
