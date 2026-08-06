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

#ifndef SRC_SOCOM_TEST_UNIT2_FRAMEWORK_INC_SINGLE_CONNECTION_TEST_FIXTURE
#define SRC_SOCOM_TEST_UNIT2_FRAMEWORK_INC_SINGLE_CONNECTION_TEST_FIXTURE

#include "gtest/gtest.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/connector_factory.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/vector_payload.hpp"

namespace score::socom
{

/// \brief Small payload
const Payload& input_data();

/// \brief Small payload
const Payload& error_data();

/// \brief SingleConnectionTest provides some constants which are almost always
///        in each test
class SingleConnectionTest : public ::testing::Test
{
  public:
    const Service_interface_identifier service_interface{
        Service_interface_identifier{"TestInterface1", Literal_tag{}, Service_interface_identifier::Version{1U, 2U}}};
    const Service_instance service_instance{"TestInterface1", Literal_tag{}};
    std::size_t num_methods{2U};
    std::size_t num_events{3U};
    Connector_factory connector_factory{service_interface,
                                        to_num_of_methods(num_methods),
                                        to_num_of_events(num_events),
                                        service_instance};
    static const Method_id method_id{0x01};
    const Method_id min_method_id{0};
    const Method_id max_method_id{static_cast<Method_id>(connector_factory.get_num_methods() - 1)};
    static const Event_id event_id{0x02};
    const Event_id min_event_id{0};
    const Event_id max_event_id{static_cast<Event_id>(connector_factory.get_num_events() - 1)};
    const Payload real_payload = make_vector_payload(make_vector_buffer(1U, 2U, 3U, 4U));
};

}  // namespace score::socom

#endif  // SRC_SOCOM_TEST_UNIT2_FRAMEWORK_INC_SINGLE_CONNECTION_TEST_FIXTURE
