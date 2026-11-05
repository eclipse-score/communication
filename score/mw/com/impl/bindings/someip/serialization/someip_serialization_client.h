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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SOMEIP_SERIALIZATION_CLIENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SOMEIP_SERIALIZATION_CLIENT_H

#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/serialization/serialization_service.h"
#include "score/mw/com/impl/serialization/serializer.h"

#include "score/result/result.h"

#include <memory>

namespace score::mw::com::impl::someip::serialization
{

/// \brief Thin SOME/IP binding entry point that opts in to serialization.
///
/// This is the binding-specific side of the serialization design. It deliberately does *nothing*
/// except opt in: it builds a `SerializationRequest` for an instance and asks the binding-independent
/// `SerializationService` for a serializer. It never touches the dynamic loader, symbol resolution,
/// the symbol cache, or the structural/attribute hashes - all of that stays in the binding-independent
/// layer.
class SomeIpSerializationClient final
{
  public:
    /// \param serialization_service The binding-independent service that performs the full
    ///        serializer selection and creation. Must outlive this client.
    explicit SomeIpSerializationClient(
        impl::serialization::SerializationService& serialization_service) noexcept;

    /// \brief Opt in to serialization for the given instance.
    ///
    /// Builds the opt-in request and forwards it to the binding-independent service. The deployment
    /// configuration discovered through the `InstanceSpecifier` drives which serializer is selected.
    /// \return The serializer to use, or the failure reported by the binding-independent layer.
    score::Result<std::unique_ptr<impl::serialization::ISerializer>> OptInForSerialization(
        const InstanceSpecifier& instance_specifier) const noexcept;

  private:
    impl::serialization::SerializationService* serialization_service_;
};

}  // namespace score::mw::com::impl::someip::serialization

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SERIALIZATION_SOMEIP_SERIALIZATION_CLIENT_H
