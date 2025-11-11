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
#include <gtest/gtest.h>

#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"

#include <score/assert.hpp>

namespace score::message_passing
{
namespace
{

void stderr_handler(const score::cpp::handler_parameters& param)
{
    std::cerr << "In " << param.file << ":" << param.line << " " << param.function << " condition " << param.condition
              << " >> " << param.message << std::endl;
}

using namespace ::testing;

TEST(QnxResourcePathDeathTest, EmptyIdentifier)
{
    score::cpp::set_assertion_handler(stderr_handler);

    EXPECT_DEATH(detail::QnxResourcePath{""}, "identifier.size");
}

TEST(QnxResourcePathDeathTest, IdentifierTooBig)
{
    score::cpp::set_assertion_handler(stderr_handler);

    std::string identifier(detail::QnxResourcePath::kMaxIdentifierLen + 1, 'f');
    EXPECT_DEATH(detail::QnxResourcePath{identifier}, "identifier.size");
}

TEST(QnxResourcePathTest, IdentifierJustEnough)
{
    std::string identifier(detail::QnxResourcePath::kMaxIdentifierLen, 's');
    std::string expected_path(std::string{detail::GetQnxPrefix()} + identifier);
    EXPECT_STREQ(detail::QnxResourcePath{identifier}.c_str(), expected_path.c_str());
}

TEST(QnxResourcePathTest, IdentifierStartsWithSlash)
{
    std::string slash{"/"};
    std::string identifier(detail::QnxResourcePath::kMaxIdentifierLen - 1, 's');
    std::string expected_path(std::string{detail::GetQnxPrefix()} + identifier);
    EXPECT_STREQ(detail::QnxResourcePath{slash + identifier}.c_str(), expected_path.c_str());
}

}  // namespace
}  // namespace score::message_passing
