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
#ifndef SCORE_MW_COM_IMPL_ENRICHED_INSTANCE_IDENTIFIER_H
#define SCORE_MW_COM_IMPL_ENRICHED_INSTANCE_IDENTIFIER_H

#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"

#include <score/assert.hpp>
#include <score/optional.hpp>

#include <utility>

namespace score::mw::com::impl
{

/// \brief Mutable wrapper class around an InstanceIdentifier which allows to modify different attributes
///
/// Difference between EnrichedInstanceIdentifier, InstanceIdentifier and HandleType:
///     - InstanceIdentifier: Non-mutable object which is generated purely from the configuration. It contains an
///     optional ServiceInstanceId which is set in the general case and is not set when it is used for a FindAny search.
///     - HandleType: Contains an InstanceIdentifier. Also contains a ServiceInstanceId which is filled on construction
///     by the ServiceInstanceId from the InstanceIdentifier if it has one, otherwise, by a ServiceInstanceId that is
///     passed into the constructor (which would be found in the FindAny search). A HandleType must always contain a
///     valid ServiceInstanceId.
///     - EnrichedInstanceIdentifier: Allows overwriting of some internal attributes instance identifiers.
class EnrichedInstanceIdentifier final
{
  public:
    explicit EnrichedInstanceIdentifier(InstanceIdentifier instance_identifier) noexcept
        : EnrichedInstanceIdentifier(
              InstanceIdentifierView{instance_identifier}.GetServiceInstanceId(),
              InstanceIdentifierView{instance_identifier}.GetServiceInstanceDeployment().asilLevel_,
              instance_identifier)
    {
    }

    // Suppress "AUTOSAR C++14 A12-6-1" rule finding. This rule declares: "All class data members that are
    // initialized by the constructor shall be initialized using member initializers".
    // This is false positive, all data members are initialized using member initializers in the delegated constructor.
    // coverity[autosar_cpp14_a12_6_1_violation]
    EnrichedInstanceIdentifier(InstanceIdentifier instance_identifier, const ServiceInstanceId instance_id) noexcept
        : EnrichedInstanceIdentifier(
              score::cpp::optional<ServiceInstanceId>{instance_id},
              InstanceIdentifierView{instance_identifier}.GetServiceInstanceDeployment().asilLevel_,
              instance_identifier)
    {
        const bool config_contains_instance_id =
            InstanceIdentifierView{instance_identifier_}.GetServiceInstanceId().has_value();
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
            !config_contains_instance_id,
            "An ServiceInstanceId should only be provided to EnrichedInstanceIdentifier if "
            "one doesn't exist in the config.");
    }

    EnrichedInstanceIdentifier(EnrichedInstanceIdentifier instance_identifier, const QualityType quality_type) noexcept
        : EnrichedInstanceIdentifier(instance_identifier.instance_id_,
                                     quality_type,
                                     std::move(instance_identifier.instance_identifier_))
    {
    }

    explicit EnrichedInstanceIdentifier(const HandleType handle) noexcept
        : EnrichedInstanceIdentifier(
              handle.GetInstanceId(),
              InstanceIdentifierView{handle.GetInstanceIdentifier()}.GetServiceInstanceDeployment().asilLevel_,
              handle.GetInstanceIdentifier())
    {
    }

    EnrichedInstanceIdentifier(score::cpp::optional<ServiceInstanceId> instance_id,
                               QualityType quality_type,
                               InstanceIdentifier instance_identifier) noexcept
        : instance_identifier_{std::move(instance_identifier)}, instance_id_{instance_id}, quality_type_{quality_type}
    {
    }

    const InstanceIdentifier& GetInstanceIdentifier() const noexcept
    {
        return instance_identifier_;
    }

    template <typename ServiceTypeDeployment>

    // Suppress "AUTOSAR C++14 M3-2-2", The rule states: "The One Definition Rule shall not be violated."
    // The GetBindingSpecificServiceId class is a templated function, each translation unit will instantiate
    // it separately. This can not be avoided.
    // coverity[autosar_cpp14_m3_2_2_violation]
    score::cpp::optional<typename ServiceTypeDeployment::ServiceId> GetBindingSpecificServiceId() const noexcept
    {
        const InstanceIdentifierView instance_identifier_view{instance_identifier_};
        const auto* service_deployment =
            std::get_if<ServiceTypeDeployment>(&(instance_identifier_view.GetServiceTypeDeployment().binding_info_));
        if (service_deployment == nullptr)
        {
            return score::cpp::nullopt;
        }
        return service_deployment->service_id_;
    }

    const score::cpp::optional<ServiceInstanceId>& GetInstanceId() const noexcept
    {
        return instance_id_;
    }

    template <typename ServiceInstanceId>
    // Suppress "AUTOSAR C++14 M3-2-2", The rule states: "The One Definition Rule shall not be violated."
    // The GetBindingSpecificInstanceId class is a templated function, each translation unit will instantiate
    // it separately. This can not be avoided.
    // coverity[autosar_cpp14_m3_2_2_violation]
    score::cpp::optional<typename ServiceInstanceId::InstanceId> GetBindingSpecificInstanceId() const noexcept
    {
        if (!instance_id_.has_value())
        {
            return score::cpp::nullopt;
        }

        const auto* instance_id = std::get_if<ServiceInstanceId>(&(instance_id_->binding_info_));
        if (instance_id == nullptr)
        {
            return score::cpp::nullopt;
        }
        return instance_id->GetId();
    }

    QualityType GetQualityType() const noexcept
    {
        return quality_type_;
    }

  private:
    InstanceIdentifier instance_identifier_;
    score::cpp::optional<ServiceInstanceId> instance_id_;
    QualityType quality_type_;
};

bool operator==(const EnrichedInstanceIdentifier& lhs, const EnrichedInstanceIdentifier& rhs) noexcept;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_ENRICHED_INSTANCE_IDENTIFIER_H
