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
#ifndef SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_CONSUMER_H
#define SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_CONSUMER_H

#include <score/stop_token.hpp>

namespace score::mw::com::test
{

/// \brief Runs the (scenario-agnostic) consumer. It always connects to kInstanceSpecifierMovedTo, since regardless
/// of scenario (move-construct/move-assign, before/after offer), the object which ends up being offered under that
/// instance specifier is the one the consumer needs to talk to.
void RunConsumer(const score::cpp::stop_token& stop_token);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_SKELETON_FIELD_MOVE_SEMANTICS_CONSUMER_H
