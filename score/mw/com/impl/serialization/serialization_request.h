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
#ifndef SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_REQUEST_H
#define SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_REQUEST_H

#include "score/mw/com/impl/instance_specifier.h"

namespace score::mw::com::impl::serialization
{

/// \brief Opt-in request a binding uses to state that it needs serialization.
///
/// A binding does not perform serialization itself. It only opts in by creating this request and
/// handing it to the binding-independent layer. The request carries nothing but the
/// `InstanceSpecifier` of the service/instance the binding is working with.
///
/// Every other input required to select and perform serialization is intentionally NOT part of this
/// type. In particular:
///   - the structural hash is derived from the service interface,
///   - the attribute hash is derived from the deployment configuration (discovered through the
///     `InstanceSpecifier`),
///   - the serializer library lifecycle (dlopen/dlsym/dlclose) and the symbol cache stay inside the
///     binding-independent layer.
///
/// This keeps the binding thin: it says "I need serialization for this instance" and the
/// binding-independent layer takes care of the full serialization flow.
class SerializationRequest final
{
  public:
    /// \brief Create an opt-in request for the given instance.
    /// \param instance_specifier The instance the binding wants serialization for. The
    ///        binding-independent layer uses it to look up the deployment configuration that drives
    ///        serializer selection.
    explicit SerializationRequest(InstanceSpecifier instance_specifier) noexcept;

    /// \brief Access the instance the binding opted in for.
    /// \return The `InstanceSpecifier` the binding-independent layer uses to resolve the deployment
    ///         configuration for serializer selection.
    const InstanceSpecifier& GetInstanceSpecifier() const noexcept;

  private:
    InstanceSpecifier instance_specifier_;
};

}  // namespace score::mw::com::impl::serialization

#endif  // SCORE_MW_COM_IMPL_SERIALIZATION_SERIALIZATION_REQUEST_H
