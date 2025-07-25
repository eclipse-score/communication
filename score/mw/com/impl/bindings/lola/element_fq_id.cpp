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
#include "score/mw/com/impl/bindings/lola/element_fq_id.h"

#include "score/mw/log/logging.h"

#include <cstdint>
#include <exception>
#include <limits>
#include <string>

namespace score::mw::com::impl::lola
{

ElementFqId::ElementFqId() noexcept
    : ElementFqId{std::numeric_limits<std::uint16_t>::max(),
                  std::numeric_limits<std::uint16_t>::max(),
                  std::numeric_limits<std::uint16_t>::max(),
                  ServiceElementType::INVALID}
{
}

ElementFqId::ElementFqId(const std::uint16_t service_id,
                         const std::uint16_t element_id,
                         const std::uint16_t instance_id,
                         const std::uint8_t element_type) noexcept
    // Suppress "AUTOSAR C++14 A7-2-1" rule: "An expression with enum underlying type shall only have values
    // corresponding to the enumerators of the enumeration.".
    // The enumeration cast "element_type" to "ServiceElementType" is checked below for compliance with the enumeration
    // range coverity[autosar_cpp14_a7_2_1_violation]
    : ElementFqId(service_id, element_id, instance_id, static_cast<ServiceElementType>(element_type))
{
    if (element_type > static_cast<std::uint8_t>(ServiceElementType::FIELD))
    {
        score::mw::log::LogFatal("lola") << "ElementFqId::ElementFqId failed: Invalid ServiceElementType:"
                                       << element_type;
        std::terminate();
    }
}

ElementFqId::ElementFqId(const std::uint16_t service_id,
                         const std::uint16_t element_id,
                         const std::uint16_t instance_id,
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

bool IsElementEvent(const ElementFqId& element_fq_id) noexcept
{
    return element_fq_id.element_type_ == ServiceElementType::EVENT;
}

bool IsElementField(const ElementFqId& element_fq_id) noexcept
{
    return element_fq_id.element_type_ == ServiceElementType::FIELD;
}

bool operator==(const ElementFqId& lhs, const ElementFqId& rhs) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-6" rule finding. This rule states:"The operands of a logical && or \\ shall be
    // parenthesized if the operands contain binary operators".
    // This a false-positive, all operands are parenthesized.
    // A bug ticket has been created to track this: [Ticket-165315](broken_link_j/Ticket-165315)
    // coverity[autosar_cpp14_a5_2_6_violation : FALSE]
    return ((lhs.service_id_ == rhs.service_id_) && (lhs.element_id_ == rhs.element_id_) &&
            (lhs.instance_id_ == rhs.instance_id_));
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

// Suppress "AUTOSAR C++14 A13-2-2" rule finding: "A binary arithmetic operator and a bitwise operator shall return
// a “prvalue”."
// This is a false positive, the following operator is not arithmetic nor bitwise operator.
// operator<< is overloaded as a member function of score::mw::log::LogStream, for logging the respective types.
// coverity[autosar_cpp14_a13_2_2_violation : FALSE]
::score::mw::log::LogStream& operator<<(::score::mw::log::LogStream& log_stream, const ElementFqId& element_fq_id)
{
    log_stream << element_fq_id.ToString();
    return log_stream;
}

}  // namespace score::mw::com::impl::lola
