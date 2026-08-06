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
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include <score/socom/vector_payload.hpp>

namespace score::socom
{

const Payload& input_data()
{
    static const Payload data = make_vector_payload(make_vector_buffer(9U, 0U, 0U, 1U));
    return data;
}

const Payload& error_data()
{
    static const Payload data = make_vector_payload(make_vector_buffer(1U, 0U, 0U, 6U));
    return data;
}

const Method_id SingleConnectionTest::method_id;
const Event_id SingleConnectionTest::event_id;

}  // namespace score::socom
