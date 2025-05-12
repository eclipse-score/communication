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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_TYPE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_TYPE_DEPLOYMENT_H

#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"

#include "score/json/json_parser.h"

#include <score/string_view.hpp>

#include <cstdint>
#include <string>
#include <variant>

namespace score::mw::com::impl
{

class ServiceTypeDeployment
{
  public:
    using BindingInformation = std::variant<LolaServiceTypeDeployment, score::cpp::blank>;

    explicit ServiceTypeDeployment(const score::json::Object& json_object) noexcept;

    explicit ServiceTypeDeployment(const BindingInformation binding) noexcept;

    score::json::Object Serialize() const noexcept;
    std::string_view ToHashString() const noexcept;

    // This variable has no invariance that needs to be conserved and is needed to be both accessed and set by the user
    // thus would require a getter and setter that do not conserve any other invariants. In such a case direct access
    // is justified.
    // coverity[autosar_cpp14_a0_1_1_violation]
    // coverity[autosar_cpp14_m11_0_1_violation] same justification as the above supperssion
    BindingInformation binding_info_;

    /**
     * \brief The size of the hash string returned by ToHashString()
     *
     * The size is the max size of the hash string returned by ToHashString() from all the bindings in
     * BindingInformation plus 1 for the index of the binding type in the variant, BindingInformation.
     */
    // Variable is used in a test case -> so this line is tested and prepared for easier reuse
    // coverity[autosar_cpp14_a0_1_1_violation]
    constexpr static std::size_t hashStringSize{LolaServiceTypeDeployment::hashStringSize + 1U};

    constexpr static std::uint32_t serializationVersion = 1U;

  private:
    /**
     * \brief Stringified format of this ServiceTypeDeployment which can be used for hashing.
     */
    std::string hash_string_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SERVICE_TYPE_DEPLOYMENT_H
