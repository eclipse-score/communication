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


#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_INTERFACE_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_INTERFACE_H

#include <cstdint>
#include <vector>

namespace bmw
{
namespace mw
{
namespace com
{
namespace test
{

template <typename T>
class TestInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t> test_field{*this, "test_field"};
};

}  // namespace test
}  // namespace com
}  // namespace mw
}  // namespace bmw

#endif  // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_INTERFACE_H
