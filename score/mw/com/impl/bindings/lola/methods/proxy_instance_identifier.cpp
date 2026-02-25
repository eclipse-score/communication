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
#include "score/mw/com/impl/bindings/lola/methods/proxy_instance_identifier.h"

namespace score::mw::com::impl::lola
{

bool operator==(const ProxyInstanceIdentifier& lhs, const ProxyInstanceIdentifier& rhs) noexcept
{
    return ((lhs.process_identifier == rhs.process_identifier) &&
            (lhs.proxy_instance_counter == rhs.proxy_instance_counter));
}

mw::log::LogStream& operator<<(score::mw::log::LogStream& stream, const ProxyInstanceIdentifier& value) noexcept
{
    stream << "Application ID:" << value.process_identifier
           << ". Proxy Instance Counter:" << value.proxy_instance_counter;
    return stream;
}

}  // namespace score::mw::com::impl::lola
