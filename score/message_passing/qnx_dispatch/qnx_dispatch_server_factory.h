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
#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_SERVER_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_SERVER_FACTORY_H

#include "score/message_passing/i_server_factory.h"

namespace score
{
namespace message_passing
{

class QnxDispatchEngine;

// coverity[autosar_cpp14_a2_10_4_violation] false-positive: neither static function nor static non-member object
class QnxDispatchServerFactory final : public IServerFactory
{
  public:
    explicit QnxDispatchServerFactory(
        score::cpp::pmr::memory_resource* const resource = score::cpp::pmr::get_default_resource()) noexcept;
    explicit QnxDispatchServerFactory(const std::shared_ptr<QnxDispatchEngine> engine) noexcept;
    ~QnxDispatchServerFactory() noexcept;

    QnxDispatchServerFactory(const QnxDispatchServerFactory&) = delete;
    QnxDispatchServerFactory(QnxDispatchServerFactory&&) = delete;
    QnxDispatchServerFactory& operator=(const QnxDispatchServerFactory&) = delete;
    QnxDispatchServerFactory& operator=(QnxDispatchServerFactory&&) = delete;

    // coverity[autosar_cpp14_m11_0_1_violation] false-positive: intentional public API
    score::cpp::pmr::unique_ptr<IServer> Create(const ServiceProtocolConfig& protocol_config,
                                         const ServerConfig& server_config) noexcept override;

    std::shared_ptr<QnxDispatchEngine> GetEngine() const noexcept
    {
        return engine_;
    }

  private:
    const std::shared_ptr<QnxDispatchEngine> engine_;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_SERVER_FACTORY_H
