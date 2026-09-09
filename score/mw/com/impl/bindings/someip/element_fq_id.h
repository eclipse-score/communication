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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_ELEMENT_FQ_ID_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_ELEMENT_FQ_ID_H

#include "score/mw/com/impl/configuration/someip_service_element_id.h"
#include "score/mw/com/impl/configuration/someip_service_id.h"
#include "score/mw/com/impl/configuration/someip_service_instance_id.h"
#include "score/mw/com/impl/service_element_type.h"

#include <cstdint>
#include <functional>
#include <string>

namespace score::mw::com::impl::someip
{

/// \brief Unique identification of a service element (event, field) instance of the SOME/IP binding.
///
/// \details Mirrors lola::ElementFqId: identification consists of service id, instance id, the id of the element
///          within that service and the type of the element.
class ElementFqId
{
  public:
    using ServiceId = SomeIpServiceId;
    using ElementId = SomeIpServiceElementId;
    using InstanceId = SomeIpServiceInstanceId::InstanceId;

    /// \brief Default ctor initializing all members to their related max value, i.e. an "invalid" ElementFqId.
    ElementFqId() noexcept;
    ElementFqId(const ServiceId service_id,
                const ElementId element_id,
                const InstanceId instance_id,
                const ServiceElementType element_type) noexcept;

    std::string ToString() const noexcept;

    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceId service_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    ElementId element_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    InstanceId instance_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    ServiceElementType element_type_;
};

bool operator==(const ElementFqId& lhs, const ElementFqId& rhs) noexcept;
bool operator<(const ElementFqId& lhs, const ElementFqId& rhs) noexcept;

}  // namespace score::mw::com::impl::someip

namespace std
{

template <>
// Suppress "AUTOSAR C++14 A11-0-2" rule finding. This rule states: "A type defined as struct shall: (1) provide only
// public data members, (2) not provide any special member functions or methods, ...".
// Specializing std::hash is the idiomatic way to make a user-defined type usable as a key of the standard
// associative containers.
// coverity[autosar_cpp14_a11_0_2_violation]
struct hash<score::mw::com::impl::someip::ElementFqId>
{
    std::size_t operator()(const score::mw::com::impl::someip::ElementFqId& element_fq_id) const noexcept;
};

}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_ELEMENT_FQ_ID_H
