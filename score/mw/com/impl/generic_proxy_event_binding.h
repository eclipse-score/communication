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

#ifndef SCORE_MW_COM_IMPL_GENERIC_PROXY_EVENT_BINDING_H
#define SCORE_MW_COM_IMPL_GENERIC_PROXY_EVENT_BINDING_H

#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/sample_reference_tracker.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime.h"

namespace score::mw::com::impl
{

/// \brief Interface a generic proxy event binding implementation has to implement.
///
/// \details GenericProxyEventBinding is also fully type-erased like the ProxyEventBinding, but has some additional
/// functionality on top of it, thus it subclasses ProxyEventBinding and adds some additional methods.
/// All generic proxy event binding implementations are required to derive from this class.
/// The additional functionality GenericProxyEventBinding provides, comes mainly from gateway use-cases. I.e., we
/// see the usage of GenericProxyEventBinding mainly in gateway scenarios.
class GenericProxyEventBinding : public ProxyEventBinding
{
  public:
    /// \brief return the size and alignment information of the underlying event sample data type.
    virtual memory::DataTypeSizeInfo GetDataTypeSizeInfo() const = 0;

    /// \brief reports, whether the event sample data the SamplePtr<void> points to is in some internal serialized
    ///        format (true) or it is the binary representation of the underlying C++ data type (false).
    /// \return true in case the sample data is in some serialized format, false else.
    virtual bool HasSerializedFormat() const noexcept = 0;

  protected:
    GenericProxyEventBinding() = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_PROXY_EVENT_BINDING_H
