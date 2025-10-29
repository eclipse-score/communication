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
#include "score/mw/com/impl/tracing/tracing_runtime.h"

#include "score/analysis/tracing/common/interface_types/types.h"
#include "score/analysis/tracing/generic_trace_library/interface_types/ara_com_meta_info.h"
#include "score/analysis/tracing/generic_trace_library/interface_types/error_code/error_code.h"
#include "score/analysis/tracing/generic_trace_library/interface_types/generic_trace_api.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/mw/com/impl/tracing/trace_error.h"

#include <score/assert.hpp>
#include <score/overload.hpp>

#include <utility>

namespace score::mw::com::impl::tracing
{

namespace
{

bool IsTerminalFatalError(const score::result::Error error) noexcept
{
    using TracingErrorCodeType = std::underlying_type<analysis::tracing::ErrorCode>::type;

    return (*error == static_cast<TracingErrorCodeType>(analysis::tracing::ErrorCode::kTerminalFatal));
}

bool IsNonRecoverableError(const score::result::Error error)
{
    using ErrorCode = analysis::tracing::ErrorCode;
    // Suppress "AUTOSAR C++14 A7-2-1" rule finding. This rule states: "An expression with enum underlying type
    // shall only have values corresponding to the enumerators of the enumeration.".
    // Potential cast to undefined enum value is tolerated as IsErrorRecoverable() internally checks for undefined enum
    // values and will return false if one is encountered (error is considered fatal)
    // coverity[autosar_cpp14_a7_2_1_violation]
    return !(analysis::tracing::IsErrorRecoverable(static_cast<ErrorCode>(*error)));
}

const std::unordered_map<ProxyEventTracePointType, analysis::tracing::TracePointType>
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.".
    // std::unordered_map doesn't have a constexpr constructor.
    // coverity[autosar_cpp14_a3_3_2_violation]
    kProxyEventTracePointToTracingTracePointMap{
        {ProxyEventTracePointType::SUBSCRIBE, analysis::tracing::TracePointType::kProxyEventSub},
        {ProxyEventTracePointType::UNSUBSCRIBE, analysis::tracing::TracePointType::kProxyEventUnsub},
        {ProxyEventTracePointType::SUBSCRIBE_STATE_CHANGE,
         analysis::tracing::TracePointType::kProxyEventSubstateChange},
        {ProxyEventTracePointType::SET_SUBSCRIPTION_STATE_CHANGE_HANDLER,
         analysis::tracing::TracePointType::kProxyEventSetChghdl},
        {ProxyEventTracePointType::UNSET_SUBSCRIPTION_STATE_CHANGE_HANDLER,
         analysis::tracing::TracePointType::kProxyEventUnsetChghdl  },
        {ProxyEventTracePointType::SUBSCRIPTION_STATE_CHANGE_HANDLER_CALLBACK,
         analysis::tracing::TracePointType::kProxyEventChghdl},
        {ProxyEventTracePointType::SET_RECEIVE_HANDLER, analysis::tracing::TracePointType::kProxyEventSetRechdl},
        {ProxyEventTracePointType::UNSET_RECEIVE_HANDLER, analysis::tracing::TracePointType::kProxyEventUnsetRechdl},
        {ProxyEventTracePointType::RECEIVE_HANDLER_CALLBACK, analysis::tracing::TracePointType::kProxyEventRechdl},
        {ProxyEventTracePointType::GET_NEW_SAMPLES, analysis::tracing::TracePointType::kProxyEventGetSamples},
        {ProxyEventTracePointType::GET_NEW_SAMPLES_CALLBACK, analysis::tracing::TracePointType::kProxyEventSampleCb},
    };

const std::unordered_map<ProxyFieldTracePointType, analysis::tracing::TracePointType>
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.".
    // std::unordered_map doesn't have a constexpr constructor.
    // coverity[autosar_cpp14_a3_3_2_violation]
    kProxyFieldTracePointToTracingTracePointMap{
        {ProxyFieldTracePointType::SUBSCRIBE, analysis::tracing::TracePointType::kProxyFieldSub},
        {ProxyFieldTracePointType::UNSUBSCRIBE, analysis::tracing::TracePointType::kProxyFieldUnsub},
        {ProxyFieldTracePointType::SUBSCRIBE_STATE_CHANGE,
         analysis::tracing::TracePointType::kProxyFieldSubstateChange},
        {ProxyFieldTracePointType::SET_SUBSCRIPTION_STATE_CHANGE_HANDLER,
         analysis::tracing::TracePointType::kProxyFieldSetChghdl},
        {ProxyFieldTracePointType::UNSET_SUBSCRIPTION_STATE_CHANGE_HANDLER,
         analysis::tracing::TracePointType::kProxyFieldUnsetChghdl},
        {ProxyFieldTracePointType::SUBSCRIPTION_STATE_CHANGE_HANDLER_CALLBACK,
         analysis::tracing::TracePointType::kProxyFieldChghdl},
        {ProxyFieldTracePointType::SET_RECEIVE_HANDLER, analysis::tracing::TracePointType::kProxyFieldSetRechdl},
        {ProxyFieldTracePointType::UNSET_RECEIVE_HANDLER, analysis::tracing::TracePointType::kProxyFieldUnsetRechdl},
        {ProxyFieldTracePointType::RECEIVE_HANDLER_CALLBACK, analysis::tracing::TracePointType::kProxyFieldRechdl},
        {ProxyFieldTracePointType::GET_NEW_SAMPLES, analysis::tracing::TracePointType::kProxyFieldGetSamples},
        {ProxyFieldTracePointType::GET_NEW_SAMPLES_CALLBACK, analysis::tracing::TracePointType::kProxyFieldSampleCb},
        {ProxyFieldTracePointType::GET, analysis::tracing::TracePointType::kProxyFieldGet},
        {ProxyFieldTracePointType::GET_RESULT, analysis::tracing::TracePointType::kProxyFieldGetResult},
        {ProxyFieldTracePointType::SET, analysis::tracing::TracePointType::kProxyFieldSet},
        {ProxyFieldTracePointType::SET_RESULT, analysis::tracing::TracePointType::kProxyFieldSetResult},
    };

const std::unordered_map<SkeletonEventTracePointType, analysis::tracing::TracePointType>
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.".
    // std::unordered_map doesn't have a constexpr constructor.
    // coverity[autosar_cpp14_a3_3_2_violation]
    kSkeletonEventTracePointToTracingTracePointMap{
        {SkeletonEventTracePointType::SEND, analysis::tracing::TracePointType::kSkelEventSnd},
        {SkeletonEventTracePointType::SEND_WITH_ALLOCATE, analysis::tracing::TracePointType::kSkelEventSndA},
    };

const std::unordered_map<SkeletonFieldTracePointType, analysis::tracing::TracePointType>
    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.".
    // std::unordered_map doesn't have a constexpr constructor.
    // coverity[autosar_cpp14_a3_3_2_violation]
    kSkeletonFieldTracePointToTracingTracePointMap{
        {SkeletonFieldTracePointType::UPDATE, analysis::tracing::TracePointType::kSkelFieldUpd},
        {SkeletonFieldTracePointType::UPDATE_WITH_ALLOCATE, analysis::tracing::TracePointType::kSkelFieldUpdA},
        {SkeletonFieldTracePointType::GET_CALL, analysis::tracing::TracePointType::kSkelFieldGetCall},
        {SkeletonFieldTracePointType::GET_CALL_RESULT, analysis::tracing::TracePointType::kSkelFieldGetCallResult},
        {SkeletonFieldTracePointType::SET_CALL, analysis::tracing::TracePointType::kSkelFieldSetCall},
        {SkeletonFieldTracePointType::SET_CALL_RESULT, analysis::tracing::TracePointType::kSkelFieldSetCallResult},
    };

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
analysis::tracing::TracePointType InternalToExternalTracePointType(
    const TracingRuntime::TracePointType& internal_trace_point_type) noexcept
{
    auto visitor = score::cpp::overload(
        [](const ProxyEventTracePointType& proxy_event_trace_point) -> analysis::tracing::TracePointType {
            if (proxy_event_trace_point == ProxyEventTracePointType::INVALID)
            {
                score::mw::log::LogFatal("lola") << "TracingRuntime: Unexpected ProxyEventTracePointType!";
                std::terminate();
            }
            return kProxyEventTracePointToTracingTracePointMap.at(proxy_event_trace_point);
        },
        [](const ProxyFieldTracePointType& proxy_field_trace_point) -> analysis::tracing::TracePointType {
            if (proxy_field_trace_point == ProxyFieldTracePointType::INVALID)
            {
                score::mw::log::LogFatal("lola") << "TracingRuntime: Unexpected ProxyFieldTracePointType!";
                std::terminate();
            }
            return kProxyFieldTracePointToTracingTracePointMap.at(proxy_field_trace_point);
        },
        [](const SkeletonEventTracePointType& skeleton_event_trace_point) -> analysis::tracing::TracePointType {
            if (skeleton_event_trace_point == SkeletonEventTracePointType::INVALID)
            {
                score::mw::log::LogFatal("lola") << "TracingRuntime: Unexpected SkeletonEventTracePointType!";
                std::terminate();
            }
            return kSkeletonEventTracePointToTracingTracePointMap.at(skeleton_event_trace_point);
        },
        [](const SkeletonFieldTracePointType& skeleton_field_trace_point) -> analysis::tracing::TracePointType {
            if (skeleton_field_trace_point == SkeletonFieldTracePointType::INVALID)
            {
                score::mw::log::LogFatal("lola") << "TracingRuntime: Unexpected SkeletonFieldTracePointType!";
                std::terminate();
            }
            return kSkeletonFieldTracePointToTracingTracePointMap.at(skeleton_field_trace_point);
        });
    return std::visit(visitor, internal_trace_point_type);
}

analysis::tracing::AraComMetaInfo CreateMetaInfo(
    const ServiceElementInstanceIdentifierView& service_element_instance_identifier,
    const TracingRuntime::TracePointType& trace_point_type,
    const score::cpp::optional<TracingRuntime::TracePointDataId> trace_point_data_id,
    const ITracingRuntimeBinding& runtime_binding) noexcept
{
    const analysis::tracing::TracePointType ext_trace_point_type = InternalToExternalTracePointType(trace_point_type);
    analysis::tracing::AraComMetaInfo result{analysis::tracing::AraComProperties(
        ext_trace_point_type,
        runtime_binding.ConvertToTracingServiceInstanceElement(service_element_instance_identifier),
        trace_point_data_id)};
    if (runtime_binding.GetDataLossFlag())
    {
        result.SetDataLossBit();
    }
    return result;
}

}  // namespace

namespace detail_tracing_runtime
{

TracingRuntimeAtomicState::TracingRuntimeAtomicState() noexcept
    : consecutive_failure_counter{0U}, is_tracing_enabled{true}
{
}

TracingRuntimeAtomicState::TracingRuntimeAtomicState(TracingRuntimeAtomicState&& other) noexcept
    // Suppress "AUTOSAR C++14 A12-8-4", The rule states: "Move constructor shall not initialize its class
    // members and base classes using copy semantics".
    // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be done
    // via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding reference.
    // std::atomics are not moveable or copyable. Therefore, the underlying data must be moved/copied.
    // The underlying types are scalars and the rule says these do not have to be moved.
    // coverity[autosar_cpp14_a12_8_4_violation : FALSE]
    // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
    : consecutive_failure_counter{other.consecutive_failure_counter.load()},
      // coverity[autosar_cpp14_a12_8_4_violation : FALSE]
      // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
      is_tracing_enabled{other.is_tracing_enabled.load()}
{
}

TracingRuntimeAtomicState& TracingRuntimeAtomicState::operator=(TracingRuntimeAtomicState&& other) noexcept
{
    // Suppress "AUTOSAR C++14 A12-8-4", The rule states: "Move constructor shall not initialize its class
    // members and base classes using copy semantics".
    // Suppress "AUTOSAR C++14 A18-9-2", The rule states: "Forwarding values to other functions shall be done
    // via: (1) std::move if the value is an rvalue reference, (2) std::forward if the value is forwarding reference.
    // std::atomics are not moveable or copyable. Therefore, the underlying data must be moved/copied.
    // The underlying types are scalars and the rule says these do not have to be moved.
    // coverity[autosar_cpp14_a12_8_4_violation : FALSE]
    // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
    consecutive_failure_counter = other.consecutive_failure_counter.load();
    // coverity[autosar_cpp14_a12_8_4_violation : FALSE]
    // coverity[autosar_cpp14_a18_9_2_violation : FALSE]
    is_tracing_enabled = other.is_tracing_enabled.load();
    return *this;
}

}  // namespace detail_tracing_runtime

void TracingRuntime::DisableTracing() noexcept
{
    score::mw::log::LogWarn("lola") << "TracingRuntime: Disabling Tracing due to call to DisableTracing.";
    atomic_state_.is_tracing_enabled = false;
}

bool TracingRuntime::IsTracingEnabled()
{
    return atomic_state_.is_tracing_enabled;
}

ServiceElementTracingData TracingRuntime::RegisterServiceElement(const BindingType binding_type,
                                                                 std::uint8_t number_of_ipc_tracing_slots) noexcept
{
    auto& runtime_binding = GetTracingRuntimeBinding(binding_type);
    return runtime_binding.RegisterServiceElement(number_of_ipc_tracing_slots);
}

ResultBlank TracingRuntime::ProcessTraceCallResult(
    const ServiceElementInstanceIdentifierView& service_element_instance_identifier,
    const analysis::tracing::TraceResult& trace_call_result,
    ITracingRuntimeBinding& tracing_runtime_binding) noexcept
{
    if (trace_call_result.has_value())
    {
        tracing_runtime_binding.SetDataLossFlag(false);
        atomic_state_.consecutive_failure_counter = 0U;
        return {};
    }

    if (IsTerminalFatalError(trace_call_result.error()))
    {
        score::mw::log::LogWarn("lola") << "TracingRuntime: Disabling Tracing because of kTerminalFatal Error: "
                                      << trace_call_result.error();
        atomic_state_.is_tracing_enabled = false;
        return MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints);
    }
    atomic_state_.consecutive_failure_counter++;
    tracing_runtime_binding.SetDataLossFlag(true);
    if (atomic_state_.consecutive_failure_counter >= MAX_CONSECUTIVE_ACCEPTABLE_TRACE_FAILURES)
    {
        score::mw::log::LogWarn("lola") << "TracingRuntime: Disabling Tracing because of max number of consecutive "
                                         "errors during call of Trace has been reached.";
        atomic_state_.is_tracing_enabled = false;
        return MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints);
    }
    if (IsNonRecoverableError(trace_call_result.error()))
    {
        score::mw::log::LogWarn("lola") << "TracingRuntime: Disabling Tracing for " << service_element_instance_identifier
                                      << " because of non-recoverable error during call of Trace(). Error: "
                                      << trace_call_result.error();
        return MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance);
    }
    return {};
}

ITracingRuntimeBinding& TracingRuntime::GetTracingRuntimeBinding(const BindingType binding_type) const noexcept
{
    const auto& search = tracing_runtime_bindings_.find(binding_type);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(search != tracing_runtime_bindings_.cend(), "No tracing runtime for given BindingType!");
    return *search->second;
}

TracingRuntime::TracingRuntime(
    std::unordered_map<BindingType, ITracingRuntimeBinding*>&& tracing_runtime_bindings) noexcept
    : ITracingRuntime{}, tracing_runtime_bindings_{std::move(tracing_runtime_bindings)}
{
    for (auto tracing_runtime_binding : tracing_runtime_bindings_)
    {
        bool success = tracing_runtime_binding.second->RegisterWithGenericTraceApi();
        if (!success)
        {
            score::mw::log::LogError("lola")
                << "TracingRuntime: Registration as Client with the GenericTraceAPI failed for binding "
                << static_cast<std::underlying_type<BindingType>::type>(tracing_runtime_binding.first)
                << ". Disable Tracing!";
            // SCR-18159752 -> disable tracing.
            atomic_state_.is_tracing_enabled = false;
        }
    }
}

void TracingRuntime::SetDataLossFlag(const BindingType binding_type) noexcept
{
    if (!atomic_state_.is_tracing_enabled)
    {
        return;
    }
    GetTracingRuntimeBinding(binding_type).SetDataLossFlag(true);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". lola::TracingRuntime::RegisterShmObject inserts an element into a std::unordered_map which can throw
// an exception if the insertion fails. In this case, we want the program to terminate and do not rely on any stack
// unwinding in case of an implicit terminate.
// coverity[autosar_cpp14_a15_5_3_violation]
void TracingRuntime::RegisterShmObject(
    const BindingType binding_type,
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view,
    const memory::shared::ISharedMemoryResource::FileDescriptor shm_object_fd,
    void* const shm_memory_start_address) noexcept
{
    if (!atomic_state_.is_tracing_enabled)
    {
        return;
    }
    auto& runtime_binding = GetTracingRuntimeBinding(binding_type);
    const auto generic_trace_api_shm_handle =
        analysis::tracing::GenericTraceAPI::RegisterShmObject(runtime_binding.GetTraceClientId(), shm_object_fd);

    if (generic_trace_api_shm_handle.has_value())
    {
        runtime_binding.RegisterShmObject(
            service_element_instance_identifier_view, generic_trace_api_shm_handle.value(), shm_memory_start_address);
    }
    else
    {
        if (IsNonRecoverableError(generic_trace_api_shm_handle.error()))
        {
            if (IsTerminalFatalError(generic_trace_api_shm_handle.error()))
            {
                score::mw::log::LogWarn("lola") << "TracingRuntime: Disabling Tracing because of kTerminalFatal Error: "
                                              << generic_trace_api_shm_handle.error();
                atomic_state_.is_tracing_enabled = false;
            }
            else
            {
                score::mw::log::LogWarn("lola")
                    << "TracingRuntime: Non-recoverable error during call of RegisterShmObject() for "
                       "ServiceElementInstanceIdentifierView: "
                    << service_element_instance_identifier_view
                    << ". Related ShmObject will not be registered any any related Trace() calls will "
                       "be suppressed. Error: "
                    << generic_trace_api_shm_handle.error();
            }
        }
        else
        {
            score::mw::log::LogInfo("lola")
                << "TracingRuntime::RegisterShmObject: Registration of ShmObject for ServiceElementInstanceIdentifier "
                << service_element_instance_identifier_view
                << " failed with recoverable error: " << generic_trace_api_shm_handle.error()
                << ". Will retry once on next Trace call referring to this ShmObject.";
            runtime_binding.CacheFileDescriptorForReregisteringShmObject(
                service_element_instance_identifier_view, shm_object_fd, shm_memory_start_address);
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". std::terminate() is called on purpose in analysis::tracing::GenericTraceAPI::RegisterShmObject ->
// DaemonCommunicator::UnregisterSharedMemoryObject -> Response::GetUnregisterSharedMemoryObject() which trys to
// get a variant by using std::get which may throws std::bad_variant_access which leds to std::terminate().
// In this case, we want the program to terminate and do not rely on any stack unwinding in case of an implicit
// terminate.
// coverity[autosar_cpp14_a15_5_3_violation]
void TracingRuntime::UnregisterShmObject(
    BindingType binding_type,
    ServiceElementInstanceIdentifierView service_element_instance_identifier_view) noexcept
{
    if (!atomic_state_.is_tracing_enabled)
    {
        return;
    }
    auto& runtime_binding = GetTracingRuntimeBinding(binding_type);
    const auto shm_object_handle_result = runtime_binding.GetShmObjectHandle(service_element_instance_identifier_view);
    if (!shm_object_handle_result.has_value())
    {
        // This shm-object was never successfully registered. Call is ok, since the upper-layer/skeleton doesn't
        // book-keep it. Still clear any eventually cached fd!
        runtime_binding.ClearCachedFileDescriptorForReregisteringShmObject(service_element_instance_identifier_view);
        return;
    }
    runtime_binding.UnregisterShmObject(service_element_instance_identifier_view);

    const auto generic_trace_api_unregister_shm_result = analysis::tracing::GenericTraceAPI::UnregisterShmObject(
        runtime_binding.GetTraceClientId(), shm_object_handle_result.value());
    if (!generic_trace_api_unregister_shm_result.has_value())
    {
        if (IsNonRecoverableError(generic_trace_api_unregister_shm_result.error()))
        {
            if (IsTerminalFatalError(generic_trace_api_unregister_shm_result.error()))
            {
                score::mw::log::LogWarn("lola")
                    << "TracingRuntime::UnregisterShmObject: Disabling Tracing because of kTerminalFatal Error: "
                    << generic_trace_api_unregister_shm_result.error();
                atomic_state_.is_tracing_enabled = false;
            }
            else
            {
                score::mw::log::LogWarn("lola")
                    << "TracingRuntime::UnregisterShmObject: Non-recoverable error during call for "
                       "ServiceElementInstanceIdentifierView: "
                    << service_element_instance_identifier_view
                    << ". Error: " << generic_trace_api_unregister_shm_result.error();
            }
        }
        else
        {
            score::mw::log::LogInfo("lola")
                << "TracingRuntime::UnregisterShmObject: Unregistering ShmObject for ServiceElementInstanceIdentifier "
                << service_element_instance_identifier_view
                << " failed with recoverable error: " << generic_trace_api_unregister_shm_result.error() << ".";
        }
    }
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank TracingRuntime::Trace(const BindingType binding_type,
                                  const ServiceElementTracingData service_element_tracing_data,
                                  const ServiceElementInstanceIdentifierView service_element_instance_identifier,
                                  const TracePointType trace_point_type,
                                  const TracePointDataId trace_point_data_id,
                                  TypeErasedSamplePtr sample_ptr,
                                  const void* const shm_data_ptr,
                                  const std::size_t shm_data_size) noexcept
{
    if (!atomic_state_.is_tracing_enabled)
    {
        return MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints);
    }
    auto& runtime_binding = GetTracingRuntimeBinding(binding_type);

    auto shm_object_handle = runtime_binding.GetShmObjectHandle(service_element_instance_identifier);
    if (!shm_object_handle.has_value())
    {
        auto cached_file_descriptor =
            runtime_binding.GetCachedFileDescriptorForReregisteringShmObject(service_element_instance_identifier);

        if (!cached_file_descriptor.has_value())
        {
            // We also have no cached file descriptor for the shm-object! This means this shm-object/the trace call
            // related to it shall be ignored
            return MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance);
        }

        // Try to re-register with cached_file_descriptor
        const memory::shared::ISharedMemoryResource::FileDescriptor shm_object_fd =
            cached_file_descriptor.value().first;
        void* const shm_memory_start_address = cached_file_descriptor.value().second;
        const auto register_shm_result =
            analysis::tracing::GenericTraceAPI::RegisterShmObject(runtime_binding.GetTraceClientId(), shm_object_fd);
        if (!register_shm_result.has_value())
        {
            if (IsTerminalFatalError(register_shm_result.error()))
            {
                score::mw::log::LogWarn("lola") << "TracingRuntime: Disabling Tracing because of kTerminalFatal Error: "
                                              << register_shm_result.error();
                atomic_state_.is_tracing_enabled = false;
                return MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints);
            }
            // register failed, we won't try any further
            runtime_binding.ClearCachedFileDescriptorForReregisteringShmObject(service_element_instance_identifier);
            score::mw::log::LogError("lola")
                << "TracingRuntime::Trace: Re-registration of ShmObject for ServiceElementInstanceIdentifier "
                << service_element_instance_identifier
                << " failed. Any Trace-Call related to this ShmObject will now be ignored!";
            // we didn't get a shm-object-handle (no valid registration) and even re-registration
            // failed, so we skip this TRACE call according to  SCR-18172392, which requires only on register-retry.
            return MakeUnexpected(TraceErrorCode::TraceErrorDisableTracePointInstance);
        }
        shm_object_handle = register_shm_result.value();
        // we re-registered successfully at GenericTraceAPI -> now also register to the binding specific runtime.
        runtime_binding.RegisterShmObject(
            service_element_instance_identifier, shm_object_handle.value(), shm_memory_start_address);
    }

    const auto shm_region_start = runtime_binding.GetShmRegionStartAddress(service_element_instance_identifier);
    // a valid shm_object_handle ... a shm_region_start should also exist!
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(shm_region_start.has_value(),
                           "No shared-memory-region start address for shm-object in tracing runtime binding!");

    const auto meta_info =
        CreateMetaInfo(service_element_instance_identifier, trace_point_type, trace_point_data_id, runtime_binding);

    // Create ShmChunkList
    analysis::tracing::SharedMemoryLocation root_chunk_memory_location{
        shm_object_handle.value(),
        static_cast<size_t>(memory::shared::SubtractPointersBytes(shm_data_ptr, shm_region_start.value()))};
    analysis::tracing::SharedMemoryChunk root_chunk{root_chunk_memory_location, shm_data_size};
    analysis::tracing::ShmDataChunkList chunk_list{root_chunk};

    const auto trace_context_id =
        runtime_binding.EmplaceTypeErasedSamplePtr(std::move(sample_ptr), service_element_tracing_data);
    if (!trace_context_id.has_value())
    {
        runtime_binding.SetDataLossFlag(true);
        return {};
    }

    const auto trace_context_id_value = trace_context_id.value();
    const auto trace_result = analysis::tracing::GenericTraceAPI::Trace(
        runtime_binding.GetTraceClientId(), meta_info, chunk_list, trace_context_id_value);
    if (!trace_result.has_value())
    {
        runtime_binding.ClearTypeErasedSamplePtr(trace_context_id_value);
    }
    return ProcessTraceCallResult(service_element_instance_identifier, trace_result, runtime_binding);
}

ResultBlank TracingRuntime::Trace(const BindingType binding_type,
                                  const ServiceElementInstanceIdentifierView service_element_instance_identifier,
                                  const TracePointType trace_point_type,
                                  const score::cpp::optional<TracePointDataId> trace_point_data_id,
                                  const void* const local_data_ptr,
                                  const std::size_t local_data_size) noexcept
{
    if (!atomic_state_.is_tracing_enabled)
    {
        return MakeUnexpected(TraceErrorCode::TraceErrorDisableAllTracePoints);
    }
    auto& runtime_binding = GetTracingRuntimeBinding(binding_type);
    const auto meta_info =
        CreateMetaInfo(service_element_instance_identifier, trace_point_type, trace_point_data_id, runtime_binding);

    // Create LocalChunkList
    const analysis::tracing::LocalDataChunk root_chunk{local_data_ptr, local_data_size};
    analysis::tracing::LocalDataChunkList chunk_list{root_chunk};

    const auto trace_result =
        analysis::tracing::GenericTraceAPI::Trace(runtime_binding.GetTraceClientId(), meta_info, chunk_list);
    return ProcessTraceCallResult(service_element_instance_identifier, trace_result, runtime_binding);
}

}  // namespace score::mw::com::impl::tracing
