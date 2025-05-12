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
#include "score/mw/com/message_passing/receiver_factory_impl.h"

#include "score/mw/com/message_passing/mqueue/mqueue_receiver_traits.h"
#include "score/mw/com/message_passing/receiver.h"

// Suppress "AUTOSAR C++14 M3-2-2" and "AUTOSAR C++14 M3-2-4", The rule states: "The One Definition Rule shall not
// be violated." and "An identifier with external linkage shall have exactly one definition.", respectively.
// Create function is templated function and is instantiated at every call, which violates the ODR.
score::cpp::pmr::unique_ptr<score::mw::com::message_passing::IReceiver>
// coverity[autosar_cpp14_m3_2_2_violation]
// coverity[autosar_cpp14_m3_2_4_violation]
score::mw::com::message_passing::ReceiverFactoryImpl::Create(const std::string_view identifier,
                                                           concurrency::Executor& executor,
                                                           const score::cpp::span<const uid_t> allowed_uids,
                                                           const ReceiverConfig& receiver_config,
                                                           score::cpp::pmr::memory_resource* const memory_resource)
{
    return score::cpp::pmr::make_unique<Receiver<MqueueReceiverTraits>>(
        memory_resource, identifier, executor, allowed_uids, receiver_config);
}
