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

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_TEST_DATATYPE_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_TEST_DATATYPE_H

#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/initial_only_interface.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/set_enabled_interface.h"
#include "score/mw/com/types.h"

namespace score::mw::com::test
{

using InitialOnlyProxy = score::mw::com::AsProxy<InitialOnlyInterface>;
using InitialOnlySkeleton = score::mw::com::AsSkeleton<InitialOnlyInterface>;

using SetEnabledProxy = score::mw::com::AsProxy<SetEnabledInterface>;
using SetEnabledSkeleton = score::mw::com::AsSkeleton<SetEnabledInterface>;

// TODO: Add a getter-enabled field interface type for dedicated "get" mode integration scenarios.

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_FIELDS_TEST_RESOURCES_TEST_DATATYPE_H
