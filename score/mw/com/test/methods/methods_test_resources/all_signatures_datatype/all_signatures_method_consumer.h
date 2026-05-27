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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_ALL_SIGNATURES_DATA_TYPE_ALL_SIGNATURES_METHOD_CONSUMER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_ALL_SIGNATURES_DATA_TYPE_ALL_SIGNATURES_METHOD_CONSUMER_H

#include "score/mw/com/test/methods/methods_test_resources/all_signatures_datatype/all_signatures_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/proxy_container.h"

#include <cstdint>
#include <string>

namespace score::mw::com::test
{

/// \brief Helper class which provides consumer side method functionality for testing
///
/// This class can be used in conjunction with MethodProvider for abstracting method-related boilerplate code out of
/// tests. The Proxy type is generated from the AllSignaturesInterface.
class AllSignaturesMethodConsumer
{
  public:
    enum class CopyMode
    {
        ZERO_COPY,
        WITH_COPY
    };

    AllSignaturesProxy& GetProxy();

    void CreateProxy(InstanceSpecifier instance_specifier, const std::string& failure_message_prefix);

    void CallMethodWithInArgsAndReturn(const std::int32_t input_argument_a,
                                       const std::int32_t input_argument_b,
                                       const CopyMode copy_mode,
                                       const std::string& failure_message_prefix);

    void CallMethodWithInArgsOnly(const std::int32_t input_argument_a,
                                  const std::int32_t input_argument_b,
                                  const CopyMode copy_mode,
                                  const std::string& failure_message_prefix);

    void CallMethodWithReturnOnly(const std::int32_t expected_return_value, const std::string& failure_message_prefix);

    void CallMethodWithoutInArgsOrReturn(const std::string& failure_message_prefix);

  private:
    ProxyContainer<AllSignaturesProxy> proxy_container_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_ALL_SIGNATURES_DATA_TYPE_ALL_SIGNATURES_METHOD_CONSUMER_H
