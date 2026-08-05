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

#include "score/socom/method.hpp"

#include <cassert>

#include "temporary_thread_id_add.hpp"

namespace score {
namespace socom {

Method_call_reply_data::Method_call_reply_data(Method_reply_callback reply_callback,
                                               std::optional<Writable_payload> reply_payload)
    : reply_callback(std::move(reply_callback)), reply_payload(std::move(reply_payload)) {
    assert(!this->reply_callback.empty());
}

void Method_call_reply_data::reply(Method_result const& method_reply) const {
    auto const stop_block_token = weak_stop_block_token.lock();
    if (stop_block_token) {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
        assert(deadlock_detector);
        Temporary_thread_id_add const tmptia{deadlock_detector->enter_callback()};
#endif
        reply_callback(method_reply);
    }
}

void Method_call_reply_data::set_block_token(Weak_reference_token weak_stop_block_token
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
                                             ,
                                             Deadlock_detector& deadlock_detector
#endif
) {
    this->weak_stop_block_token = std::move(weak_stop_block_token);
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    this->deadlock_detector = &deadlock_detector;
#endif
}

}  // namespace socom
}  // namespace score
