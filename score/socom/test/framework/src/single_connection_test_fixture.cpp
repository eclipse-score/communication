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

#include "score/socom/single_connection_test_fixture.hpp"

namespace score::socom {

Payload const& input_data() {
    static Payload const data = make_vector_payload(make_vector_buffer(9U, 0U, 0U, 1U));
    return data;
}

Payload const& error_data() {
    static Payload const data = make_vector_payload(make_vector_buffer(1U, 0U, 0U, 6U));
    return data;
}

Method_id const SingleConnectionTest::method_id;
Event_id const SingleConnectionTest::event_id;

}  // namespace score::socom
