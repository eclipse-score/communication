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
#include "score/mw/com/impl/bindings/someip/serialization/someip_serialization_client.h"

#include "score/mw/com/impl/serialization/serialization_request.h"

namespace score::mw::com::impl::someip::serialization
{

SomeIpSerializationClient::SomeIpSerializationClient(
    impl::serialization::SerializationService& serialization_service) noexcept
    : serialization_service_{&serialization_service}
{
}

score::Result<std::unique_ptr<impl::serialization::ISerializer>>
SomeIpSerializationClient::OptInForSerialization(const InstanceSpecifier& instance_specifier) const noexcept
{
    const impl::serialization::SerializationRequest request{instance_specifier};
    return serialization_service_->GetSerializer(request);
}

}  // namespace score::mw::com::impl::someip::serialization
