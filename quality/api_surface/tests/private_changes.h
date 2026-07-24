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

#include <string>
#include <vector>

namespace test
{

/// \brief A class where private members change but public API stays stable.
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
    // These private members can change without breaking the public API
    std::string name_;
    std::vector<int> history_;
    int cache_;
    void updateHistory(int value);
    void clearCache();
};

}  // namespace test
