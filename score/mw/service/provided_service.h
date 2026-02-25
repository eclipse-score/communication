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

#ifndef SCORE_MW_SERVICE_PROVIDED_SERVICE_H
#define SCORE_MW_SERVICE_PROVIDED_SERVICE_H

namespace score
{
namespace mw
{
namespace service
{

/// @brief Minimal stub base class for provided services
class ProvidedService
{
  public:
    constexpr ProvidedService() noexcept = default;
    virtual ~ProvidedService() noexcept = default;

    // Stub methods - not called in config_daemon tests
    virtual void StartService() {}
    virtual void StopService() {}

  protected:
    constexpr ProvidedService(ProvidedService&&) noexcept = default;
    constexpr ProvidedService(const ProvidedService&) noexcept = default;
    ProvidedService& operator=(ProvidedService&&) & noexcept = default;
    ProvidedService& operator=(const ProvidedService&) & noexcept = default;
};

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROVIDED_SERVICE_H
