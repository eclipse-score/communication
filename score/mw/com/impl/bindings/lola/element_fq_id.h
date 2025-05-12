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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_ELEMENT_FQ_ID_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_ELEMENT_FQ_ID_H

#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>

namespace score::mw
{
namespace log
{
// Suppress "AUTOSAR C++14 M3-2-3" rule finding. This rule declares: "A type, object or function that is used in
// multiple translation units shall be declared in one and only one file.".
// This is a forward declaration that does not vioalate this rule.
// coverity[autosar_cpp14_m3_2_3_violation]
class LogStream;
}  // namespace log

namespace com::impl::lola
{

/// \brief enum used to differentiate between difference service element types
enum class ElementType : std::uint8_t
{
    INVALID = 0,
    EVENT,
    FIELD
};

/// \brief unique identification of a service element (event, field, method) instance within one score::mw runtime/process
///
/// \detail Identification consists of the four dimensions: Service-Type (service_id), instance of service
///         (instance_id), the id of the element (element_id) within this service and an enum which tracks the type of
///         the element. The first two (service_id, element_id) are defined at generation time (ara_gen). The
///         instance_id is a deployment/runtime parameter.
class ElementFqId
{
  public:
    /// \brief default ctor initializing all members to their related max value.
    /// \details constructs an "invalid" ElementFqId.
    ElementFqId() noexcept;
    ElementFqId(const std::uint16_t service_id,
                const std::uint16_t element_id,
                const std::uint16_t instance_id,
                const std::uint8_t element_type) noexcept;

    ElementFqId(const std::uint16_t service_id,
                const std::uint16_t element_id,
                const std::uint16_t instance_id,
                const ElementType element_type) noexcept;

    std::string ToString() const noexcept;

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::uint16_t service_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::uint16_t element_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::uint16_t instance_id_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    ElementType element_type_;
};
// guarantee memcpy usage
static_assert(std::is_trivially_copyable<ElementFqId>::value,
              "ElementFqId must be trivially copyable to perform memcpy op");

bool IsElementEvent(const ElementFqId& element_fq_id) noexcept;
bool IsElementField(const ElementFqId& element_fq_id) noexcept;

// Note. Equality / comparison operators do not use ElementType since the other 3 elements already uniquely identify a
// service element.

bool operator==(const ElementFqId& lhs, const ElementFqId& rhs) noexcept;

/// \brief We need to store ElementFqId in a map, so we need to be able to sort it
bool operator<(const ElementFqId&, const ElementFqId&) noexcept;

::score::mw::log::LogStream& operator<<(::score::mw::log::LogStream& log_stream, const ElementFqId& element_fq_id);

}  // namespace com::impl::lola

}  // namespace score::mw

namespace std
{
/// \brief ElementFqId is used as a key for maps, so we need a hash func for it.
///
/// The ElementType enum is not used in the hash function since the other 3 elements already uniquely identify a service
/// element.
template <>
// NOLINTBEGIN(score-struct-usage-compliance):struct type for consistency with STL
// Suppress "AUTOSAR C++14 A11-0-2" rule finding.
// This rule states: "A type defined as struct shall: (1) provide only public data members, (2)
// not provide any special member functions or methods, (3) not be a base of
// another struct or class, (4) not inherit from another struct or class.".
// coverity[autosar_cpp14_a11_0_2_violation]
struct hash<score::mw::com::impl::lola::ElementFqId>
// NOLINTEND(score-struct-usage-compliance): see above
{
    std::size_t operator()(const score::mw::com::impl::lola::ElementFqId& element_fq_id) const noexcept
    {
        return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(element_fq_id.service_id_) << 32U) |
                                          (static_cast<std::uint64_t>(element_fq_id.element_id_) << 16U) |
                                          static_cast<std::uint64_t>(element_fq_id.instance_id_));
    }
};
}  // namespace std

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_ELEMENT_FQ_ID_H
