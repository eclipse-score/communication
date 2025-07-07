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
#ifndef SCORE_MW_COM_IMPL_TYPES_H
#define SCORE_MW_COM_IMPL_TYPES_H

#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"

#include "score/mw/com/impl/event_receive_handler.h"
#include "score/mw/com/impl/find_service_handle.h"
#include "score/mw/com/impl/find_service_handler.h"
#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/subscription_state.h"
#include "score/mw/com/impl/traits.h"

#include <vector>

/**
 * \brief The Types header file includes the data type definitions which are
 specific for the
 * ara::com API.
 *
 * \requirement SWS_CM_01018, SWS_CM_01013
 // trace: score_communication.SafeCommunication
 // trace: Lola.proxy_exec_less_creation
 */
namespace score::mw::com
{

/**
 * \brief Included data types:
 *
 * - InstanceIdentifier
 * - FindServiceHandle
 * - ServiceHandleContainer
 * - FindServiceHandler
 *
 * \requirement SWS_CM_01019
 */
using InstanceIdentifier = ::score::mw::com::impl::InstanceIdentifier;

/// \brief Identifier for an application port. Maps design to deployment.
/// \requirement SWS_CM_00350
using InstanceSpecifier = ::score::mw::com::impl::InstanceSpecifier;

/**
 * \brief The container holds a list of instance identifiers and is used as
 * a return value of the ResolveInstanceIDs method.
 *
 * \requirement SWS_CM_00319
 // trace: score_communication.abc
 */
using InstanceIdentifierContainer = std::vector<InstanceIdentifier>;

using FindServiceHandle = ::score::mw::com::impl::FindServiceHandle;

/// \brief Container with handles representing currently available service instances.
///
/// See StartFindService() for more information.
template <typename T>
using ServiceHandleContainer = ::score::mw::com::impl::ServiceHandleContainer<T>;
/// \brief Callback that notifies the callee about service availability changes.
///
/// See ProxyBase::StartFindService for more information.
template <typename T>
using FindServiceHandler = ::score::mw::com::impl::FindServiceHandler<T>;

using MethodCallProcessingMode = ::score::mw::com::impl::MethodCallProcessingMode;

/// Subscription state of a proxy event.
///
/// See ProxyEvent::GetSubscriptionStatus for slightly more information.
using SubscriptionState = ::score::mw::com::impl::SubscriptionState;

/// Carries the received data on proxy side
template <typename SampleType>
using SamplePtr = impl::SamplePtr<SampleType>;

/// Carries a reference to the allocated memory that will hold the data that is to be sent to the receivers after having
/// been filled by the sending application.
template <typename SampleType>
using SampleAllocateePtr = impl::SampleAllocateePtr<SampleType>;

/// \brief Callback for event notifications on proxy side.
/// \requirement SWS_CM_00309
using EventReceiveHandler = impl::EventReceiveHandler;

/// \brief Interpret an interface that follows our traits as proxy (cf. impl/traits.h)
template <template <class> class T>
using AsProxy = impl::AsProxy<T>;

/// \brief Interpret an interface that follows our traits as skeleton (cf. impl/traits.h)
template <template <class> class T>
using AsSkeleton = impl::AsSkeleton<T>;

/// \Brief A type erased proxy that can be used to read the raw data from a skeleton without knowing the SampleType
using GenericProxy = impl::GenericProxy;

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_IMPL_TYPES_H
