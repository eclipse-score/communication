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
#include "score/mw/com/impl/bindings/someip/element_fq_id.h"

#include <limits>

namespace score::mw::com::impl::someip
{

ElementFqId::ElementFqId() noexcept
    : ElementFqId{std::numeric_limits<ServiceId>::max(),
                  std::numeric_limits<ElementId>::max(),
                  std::numeric_limits<InstanceId>::max(),
                  ServiceElementType::INVALID}
{
}

ElementFqId::ElementFqId(const ServiceId service_id,
                         const ElementId element_id,
                         const InstanceId instance_id,
                         const ServiceElementType element_type) noexcept
    : service_id_{service_id}, element_id_{element_id}, instance_id_{instance_id}, element_type_{element_type}
{
}

std::string ElementFqId::ToString() const noexcept
{
    const std::string result = std::string("ElementFqId{S:") + std::to_string(static_cast<std::uint32_t>(service_id_)) +
                               std::string(", E:") + std::to_string(static_cast<std::uint32_t>(element_id_)) +
                               std::string(", I:") + std::to_string(static_cast<std::uint32_t>(instance_id_)) +
                               std::string(", T:") + std::to_string(static_cast<std::uint32_t>(element_type_)) +
                               std::string("}");
    return result;
}

bool operator==(const ElementFqId& lhs, const ElementFqId& rhs) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or || shall be
    // parenthesized if the operands contain binary operators". This is a false positive, all operands are
    // parenthesized.
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    return ((lhs.service_id_ == rhs.service_id_) && (lhs.element_id_ == rhs.element_id_) &&
            (lhs.instance_id_ == rhs.instance_id_) && (lhs.element_type_ == rhs.element_type_));
}

bool operator<(const ElementFqId& lhs, const ElementFqId& rhs) noexcept
{
    if (lhs.service_id_ == rhs.service_id_)
    {
        if (lhs.instance_id_ == rhs.instance_id_)
        {
            return lhs.element_id_ < rhs.element_id_;
        }
        return lhs.instance_id_ < rhs.instance_id_;
    }
    return lhs.service_id_ < rhs.service_id_;
}

}  // namespace score::mw::com::impl::someip

namespace std
{

std::size_t hash<score::mw::com::impl::someip::ElementFqId>::operator()(
    const score::mw::com::impl::someip::ElementFqId& element_fq_id) const noexcept
{
    // The four id parts are shifted into disjoint bit ranges of a 64 bit value, so that distinct ElementFqIds always
    // produce distinct hash inputs.
    const std::uint64_t combined_id =
        (static_cast<std::uint64_t>(element_fq_id.service_id_) << 48U) |
        (static_cast<std::uint64_t>(element_fq_id.instance_id_) << 32U) |
        (static_cast<std::uint64_t>(element_fq_id.element_id_) << 16U) |
        static_cast<std::uint64_t>(static_cast<std::uint8_t>(element_fq_id.element_type_));
    return std::hash<std::uint64_t>{}(combined_id);
}

}  // namespace std
