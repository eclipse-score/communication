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
#include "score/mw/com/impl/bindings/lola/methods/proxy_method_instance_identifier.h"

namespace score::mw::com::impl::lola
{

bool operator==(const ProxyMethodInstanceIdentifier& lhs, const ProxyMethodInstanceIdentifier& rhs) noexcept
{
    return ((lhs.proxy_instance_identifier == rhs.proxy_instance_identifier) && (lhs.method_id == rhs.method_id));
}

mw::log::LogStream& operator<<(score::mw::log::LogStream& stream, const ProxyMethodInstanceIdentifier& value) noexcept
{
    stream << "ProxyInstanceIdentifier:" << value.proxy_instance_identifier << ". Method ID:" << value.method_id;
    return stream;
}

}  // namespace score::mw::com::impl::lola
