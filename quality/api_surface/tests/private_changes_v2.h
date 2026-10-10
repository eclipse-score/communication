// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace test
{

/// \brief Same public API as private_changes.h::StableApi, but the private
/// implementation has changed completely.
///
/// This is the negative/stability counterpart to private_changes.h: the public
/// API surface extracted from this header MUST be identical to the one from
/// private_changes.h even though every private member differs.
/// \api
class StableApi
{
  public:
    /// \brief Default constructor.
    StableApi();

    /// \brief Process a value.
    int process(int input);

    /// \brief Get the name.
    const std::string& name() const;

  private:
    // Completely different private members compared to private_changes.h.
    // None of this may leak into the public API surface.
    std::map<std::string, std::int64_t> counters_;
    std::uint64_t revision_;
    bool dirty_;
    void recompute();
    void invalidate(std::uint64_t revision);
    std::int64_t lookup(const std::string& key) const;
};

}  // namespace test
