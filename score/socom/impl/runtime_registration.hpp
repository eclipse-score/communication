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

#ifndef SCORE_SOCOM_RUNTIME_REGISTRATION_HPP
#define SCORE_SOCOM_RUNTIME_REGISTRATION_HPP

#include <memory>

namespace score {
namespace socom {

class IRegistration {
   public:
    IRegistration() = default;
    virtual ~IRegistration() noexcept = default;

    IRegistration(IRegistration const&) = delete;
    IRegistration(IRegistration&&) = delete;

    IRegistration& operator=(IRegistration const&) = delete;
    IRegistration& operator=(IRegistration&&) = delete;
};
using Registration = std::unique_ptr<IRegistration>;

}  // namespace socom
}  // namespace score

#endif  // SCORE_SOCOM_RUNTIME_REGISTRATION_HPP
