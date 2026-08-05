/********************************************************************************
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
 ********************************************************************************/

#include "score/socom/error.hpp"

#include <gtest/gtest.h>

namespace score::socom {
namespace {

TEST(Error, MakeErrorMapsServiceNotAvailable)
{
    const score::result::Error err{MakeError(Error::runtime_error_service_not_available)};
    EXPECT_EQ(*err, static_cast<int>(Error::runtime_error_service_not_available));
    EXPECT_EQ(err.Message(), "Service not available");
    EXPECT_TRUE(err.UserMessage().empty());
}

TEST(Error, MakeErrorMapsRequestRejected)
{
    const score::result::Error err{MakeError(Error::runtime_error_request_rejected)};
    EXPECT_EQ(*err, static_cast<int>(Error::runtime_error_request_rejected));
    EXPECT_EQ(err.Message(), "Request rejected");
}

TEST(Error, MakeErrorMapsIdOutOfRange)
{
    const score::result::Error err{MakeError(Error::logic_error_id_out_of_range)};
    EXPECT_EQ(*err, static_cast<int>(Error::logic_error_id_out_of_range));
    EXPECT_EQ(err.Message(), "ID out of range");
}

TEST(Error, MakeErrorMapsMalformedPayload)
{
    const score::result::Error err{MakeError(Error::runtime_error_malformed_payload)};
    EXPECT_EQ(*err, static_cast<int>(Error::runtime_error_malformed_payload));
    EXPECT_EQ(err.Message(), "Malformed payload");
}

TEST(Error, MakeErrorMapsPermissionNotAllowed)
{
    const score::result::Error err{MakeError(Error::runtime_error_permission_not_allowed)};
    EXPECT_EQ(*err, static_cast<int>(Error::runtime_error_permission_not_allowed));
    EXPECT_EQ(err.Message(), "Permission not allowed");
}

TEST(Error, MakeErrorFallsBackToUnknownMessage)
{
    const score::result::Error err{MakeError(static_cast<Error>(-1))};
    EXPECT_EQ(err.Message(), "Unknown Error");
}

TEST(Error, MakeErrorPreservesUserMessage)
{
    constexpr std::string_view kUserMessage{"test user message"};
    const score::result::Error err{MakeError(Error::runtime_error_request_rejected, kUserMessage)};
    EXPECT_EQ(err.UserMessage(), kUserMessage);
}

TEST(ServerConnectorError, MakeErrorMapsIdOutOfRange)
{
    const score::result::Error err{MakeError(Server_connector_error::logic_error_id_out_of_range)};
    EXPECT_EQ(*err, static_cast<int>(Server_connector_error::logic_error_id_out_of_range));
    EXPECT_EQ(err.Message(), "ID out of range");
    EXPECT_TRUE(err.UserMessage().empty());
}

TEST(ServerConnectorError, MakeErrorMapsNoClientSubscribedForEvent)
{
    const score::result::Error err{MakeError(Server_connector_error::runtime_error_no_client_subscribed_for_event)};
    EXPECT_EQ(*err, static_cast<int>(Server_connector_error::runtime_error_no_client_subscribed_for_event));
    EXPECT_EQ(err.Message(), "No client subscribed for event");
}

TEST(ServerConnectorError, MakeErrorFallsBackToUnknownMessage)
{
    const score::result::Error err{MakeError(static_cast<Server_connector_error>(-1))};
    EXPECT_EQ(err.Message(), "Unknown Error");
}

TEST(ServerConnectorError, MakeErrorPreservesUserMessage)
{
    constexpr std::string_view kUserMessage{"test user message"};
    const score::result::Error err{MakeError(Server_connector_error::logic_error_id_out_of_range, kUserMessage)};
    EXPECT_EQ(err.UserMessage(), kUserMessage);
}

TEST(ConstructionError, MakeErrorMapsDuplicateService)
{
    const score::result::Error err{MakeError(Construction_error::duplicate_service)};
    EXPECT_EQ(*err, static_cast<int>(Construction_error::duplicate_service));
    EXPECT_EQ(err.Message(), "Duplicate service");
    EXPECT_TRUE(err.UserMessage().empty());
}

TEST(ConstructionError, MakeErrorMapsDuplicateClient)
{
    const score::result::Error err{MakeError(Construction_error::duplicate_client)};
    EXPECT_EQ(*err, static_cast<int>(Construction_error::duplicate_client));
    EXPECT_EQ(err.Message(), "Duplicate client");
}

TEST(ConstructionError, MakeErrorMapsCallbackMissing)
{
    const score::result::Error err{MakeError(Construction_error::callback_missing)};
    EXPECT_EQ(*err, static_cast<int>(Construction_error::callback_missing));
    EXPECT_EQ(err.Message(), "Callback missing");
}

TEST(ConstructionError, MakeErrorFallsBackToUnknownMessage)
{
    const score::result::Error err{MakeError(static_cast<Construction_error>(-1))};
    EXPECT_EQ(err.Message(), "Unknown Error");
}

TEST(ConstructionError, MakeErrorPreservesUserMessage)
{
    constexpr std::string_view kUserMessage{"test user message"};
    const score::result::Error err{MakeError(Construction_error::duplicate_client, kUserMessage)};
    EXPECT_EQ(err.UserMessage(), kUserMessage);
}

}  // namespace
}  // namespace score::socom
