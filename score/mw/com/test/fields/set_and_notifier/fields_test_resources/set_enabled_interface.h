/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_SET_ENABLED_INTERFACE_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_SET_ENABLED_INTERFACE_H

#include <cstdint>

namespace score::mw::com::test
{

template <typename T>
class SetEnabledInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t, false, true> test_field{*this, "test_field"};
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_SET_ENABLED_INTERFACE_H
