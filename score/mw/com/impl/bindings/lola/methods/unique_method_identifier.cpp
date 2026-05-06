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
#include "score/mw/com/impl/bindings/lola/methods/unique_method_identifier.h"

namespace score::mw::com::impl::lola
{

bool operator==(const UniqueMethodIdentifier& lhs, const UniqueMethodIdentifier& rhs) noexcept
{
    return ((lhs.method_or_field_id == rhs.method_or_field_id) && (lhs.method_type == rhs.method_type));
}

bool operator!=(const UniqueMethodIdentifier& lhs, const UniqueMethodIdentifier& rhs) noexcept
{
    return !(lhs == rhs);
}

mw::log::LogStream& operator<<(score::mw::log::LogStream& stream, const UniqueMethodIdentifier& value) noexcept
{
    stream << "MethodOrFieldId:" << value.method_or_field_id << ". MethodType:" << to_string(value.method_type);
    return stream;
}

}  // namespace score::mw::com::impl::lola
