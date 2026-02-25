
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

// Why registry based approach when we have static macro based approach in bridge_macro.h -->
//
//  DESIGN DECISION: Registry-Based Type Mapping for Runtime-Independent COM-API Binding
//
//  Problem with static bridge_macro.h approach:
//  - bridge_macro.h provides static, compile-time macro based implementation
//  - It generates static functions or APIs that are bound to specific types at compile time
//  - This approach requires all type information to be known and generated at compile time
//  - Static binding prevents true separation between COM-API library and runtime implementations
//  - We cannot have different runtime implementations (Lola, Mock, etc) sharing the same FFI interface
//
//  Why this file exists:
//  - This file provides registry-based implementation for skeleton and proxy classes
//  - Registry enables runtime type binding between Rust and C++ instead of compile-time static binding
//  - As COM-API is runtime independent, we need a mechanism that works with any runtime
//  - We can not rely on static macros alone to achieve runtime independence
//
//  Key design elements:
//  - GlobalRegistryMapping: Central registry that maps string identifiers to type/interface operations
//    - InterfaceOperationMap: Maps interface_id (string) -> InterfaceOperations (proxy/skeleton creation)
//    - TypeOperationMap: Maps type_name (string) -> TypeOperations (sample handling, event sending)
//
//  How it works:
//  - Registry is filled at COMPILE TIME using macros (BEGIN_EXPORT_MW_COM_INTERFACE, EXPORT_MW_COM_EVENT,
//  EXPORT_MW_COM_TYPE)
//  - Macros create static helper structs that register operations in GlobalRegistryMapping at program startup
//  - Operations are looked up at RUNTIME using string keys (interface_id, event_id, type_name)
//  - Rust calls FFI functions with string identifiers
//  - C++ resolves actual types via registry and invokes appropriate virtual methods
//
//  Example flow for event subscription:
//  - Rust calls: get_event_from_proxy(proxy_ptr, "VehicleInterface", "TireEvent")
//  - C++: FindMemberOperation("VehicleInterface", "TireEvent") -> returns MemberOperationImpl<VehicleProxy,
//  VehicleSkeleton, Tire, ...>
//  - C++: Calls GetProxyEvent() on the returned MemberOperation
//  - C++: Returns ProxyEventBase* which is actually ProxyEvent<Tire>*
//  - Rust receives opaque ProxyEventBase* and can use it with type-name strings in subsequent calls
//
//  Template specialization pattern:
//  - EXPORT_MW_COM_TYPE macro specializes RustRefMutCallable template for each type
//  - This allows C++ to invoke Rust FnMut closures with type-specific SamplePtr<T>
//  - SamplePtr<T> is wrapped in placement-new storage and passed to mw_com_impl_call_dyn_ref_fnmut_sample()
//  - The Rust side reconstructs the FatPtr and invokes the original closure
//
//  Application side usage:
//  - Generated C++ code invokes these macros to fill registry for each interface and type
//  - Macros are typically invoked in a dedicated registration compilation unit
//  - Registry is thread-safe via static initialization (runs before main)

#ifndef SCORE_MW_COM_REGISTRY_BRIDGE_MACROS_H
#define SCORE_MW_COM_REGISTRY_BRIDGE_MACROS_H

#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_event_base.h"
#include "score/mw/com/impl/rust/proxy_bridge.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace score::mw::com::impl::rust
{

/// \brief String view for FFI boundary passing strings from Rust to C++
/// \details Holds a pointer to a string and its length without requiring null termination.
/// Similar to C++'s std::string_view but optimized for FFI use.
struct StringView
{
    const char* data = nullptr;  ///< Pointer to string data
    uint32_t len = 0;            ///< Length of the string

    /// \brief Conversion operator to std::string_view
    /// \details Allows implicit conversion: std::string_view sv = static_cast<std::string_view>(string_view_instance);
    /// \return std::string_view object constructed from this StringView
    explicit operator std::string_view() const noexcept
    {
        return std::string_view(data, len);
    }
};

namespace details
{
/// \brief Get samples from ProxyEvent with type erasure
/// \details Invokes the provided callback for each sample of type T retrieved from the event.
/// \param proxy_event Reference to ProxyEvent of type T
/// \param callback FatPtr of Rust FnMut callable that takes SamplePtr<T>
/// \param max_num_samples Maximum number of samples to process
/// \return Result containing the number of samples processed on success, or error code on failure
template <typename T>
inline score::Result<std::uint32_t> GetSamplesFromEvent(::score::mw::com::impl::ProxyEvent<T>& proxy_event,
                                                      const FatPtr& callback,
                                                      std::uint32_t max_num_samples) noexcept
{
    RustFnMutCallable<RustRefMutCallable, void, SamplePtr<T>> rust_callable{callback};

    auto samples = proxy_event.GetNewSamples(std::move(rust_callable), max_num_samples);

    if (samples.has_value())
    {
        return samples;
    }
    else
    {
        return score::MakeUnexpected<std::uint32_t>(samples.error());
    }
}

}  // namespace details

/// \brief Interface for type-erased operations on types used in ProxyEvent and SkeletonEvent
/// \details Provides virtual methods to handle samples and send events for types with type erasure.
class TypeOperations
{

  public:
    virtual ~TypeOperations() = default;

    /// \brief Get samples from ProxyEvent of specific type
    /// \details Retrieves samples from a ProxyEvent and invokes the callback for each sample.
    /// \param event_ptr Pointer to ProxyEventBase instance
    /// \param max_sample Maximum number of samples to process
    /// \param callBack FatPtr of Rust FnMut callable that takes SamplePtr<T>
    /// \return Result containing the number of samples processed on success, or error code on failure
    virtual Result<uint32_t> GetSamplesFromEvent(ProxyEventBase* event_ptr, uint32_t max_sample, FatPtr callBack) = 0;

    /// \brief Send event data through SkeletonEvent of specific type
    /// \details Casts the type-erased data pointer back to the actual type and sends it via SkeletonEvent.
    /// \param event_ptr Pointer to SkeletonEventBase instance
    /// \param data_ptr Pointer to type T (erased as void*, casted back to T* in implementation)
    /// \return true if send successful, false otherwise
    virtual bool SkeletonSendEvent(SkeletonEventBase* event_ptr, void* data_ptr) = 0;

    /// \brief Get data pointer from SamplePtr of specific type
    /// \details Casts the type-erased SamplePtr back to SamplePtr<T> and retrieves the underlying data pointer.
    /// \param sample_ptr Pointer to type-erased SamplePtr
    /// \return Pointer to the underlying data of type T
    virtual const void* GetSamplePtrData(const void* sample_ptr) = 0;

    /// \brief Delete SamplePtr of specific type
    /// \details Casts the type-erased SamplePtr back to SamplePtr<T> and deletes it properly.
    /// \param sample_ptr Pointer to type-erased SamplePtr
    virtual void DeleteSamplePtr(void* sample_ptr) = 0;

    /// @brief Get allocatee pointer from SkeletonEvent of specific type
    /// \details Allocates a SampleAllocateePtr<T> from the SkeletonEvent and places it into the provided storage.
    /// \param event_ptr Pointer to SkeletonEventBase instance
    /// \param allocatee_ptr Pointer to storage for SampleAllocateePtr<T>
    /// \return true if allocation successful, false otherwise
    virtual bool GetAllocateePtr(SkeletonEventBase* event_ptr, void* allocatee_ptr) = 0;

    /// \brief Get data pointer from SampleAllocateePtr of specific type
    /// \details Casts the type-erased SampleAllocateePtr back to SampleAllocateePtr<T>
    /// \param allocatee_ptr Pointer to SampleAllocateePtr<T>
    /// \return Pointer to the underlying data of type T
    virtual void* GetAllocateeDataPtr(void* allocatee_ptr) = 0;

    /// \brief Delete SampleAllocateePtr of specific type
    /// \details Casts the type-erased SampleAllocateePtr back to SampleAllocateePtr<T> and deletes it properly.
    /// \param allocatee_ptr Pointer to type-erased SampleAllocateePtr
    virtual void DeleteAllocateePtr(void* allocatee_ptr) = 0;

    /// @brief Send allocatee data through SkeletonEvent of specific type
    /// \details Casts the type-erased allocatee pointer back to SampleAllocateePtr<T> and sends it via SkeletonEvent.
    /// \param event_ptr Pointer to SkeletonEventBase instance
    /// \param allocatee_ptr Pointer SampleAllocateePtr (of type T)
    /// \return true if send successful, false otherwise
    virtual bool SkeletonSendEventAllocatee(SkeletonEventBase* event_ptr, void* allocatee_ptr) = 0;

    // TODO: Allocate API need to add - Ticket-234824
};

/// \brief Template implementation of TypeOperations for a specific type T
/// \details Provides concrete implementations of GetSamplesFromEvent and SkeletonSendEvent methods.
/// This class is instantiated and registered for each type T via macros.
template <typename T>
class TypeOperationImpl : public TypeOperations
{
  public:
    Result<uint32_t> GetSamplesFromEvent(ProxyEventBase* event_ptr, uint32_t max_sample, FatPtr callBack) override
    {
        auto proxy_event = dynamic_cast<ProxyEvent<T>*>(event_ptr);
        if (proxy_event == nullptr)
        {
            return score::MakeUnexpected<std::uint32_t>(ComErrc::kInvalidHandle);
        }
        return details::GetSamplesFromEvent<T>(*proxy_event, callBack, max_sample);
    }

    bool SkeletonSendEvent(SkeletonEventBase* event_ptr, void* data_ptr) override
    {

        auto skeleton_event = dynamic_cast<SkeletonEvent<T>*>(event_ptr);
        if (skeleton_event == nullptr)
        {
            return false;
        }
        // Cast data_ptr back to T*
        T* typed_data = static_cast<T*>(data_ptr);
        if (typed_data == nullptr)
        {
            return false;
        }
        return skeleton_event->Send(*typed_data).has_value();
    }

    const void* GetSamplePtrData(const void* sample_ptr) override
    {
        if (sample_ptr == nullptr)
            return nullptr;

        auto* typed_ptr = static_cast<const ::score::mw::com::impl::SamplePtr<T>*>(sample_ptr);
        return typed_ptr->Get();
    }

    void DeleteSamplePtr(void* sample_ptr) override
    {
        if (sample_ptr == nullptr)
            return;

        auto* typed_ptr = static_cast<::score::mw::com::impl::SamplePtr<T>*>(sample_ptr);
        typed_ptr->~SamplePtr<T>();
    }

    bool GetAllocateePtr(SkeletonEventBase* event_ptr, void* allocatee_ptr) override
    {
        if (event_ptr == nullptr)
        {
            return false;
        }
        auto skeleton_event = dynamic_cast<SkeletonEvent<T>*>(event_ptr);
        if (skeleton_event == nullptr)
        {
            return false;
        }

        auto allocatee_ptr_ = skeleton_event->Allocate();
        if (!allocatee_ptr_.has_value())
        {
            return false;
        }

        new (allocatee_ptr) SampleAllocateePtr<T>(std::move(allocatee_ptr_).value());
        return true;
    }

    void DeleteAllocateePtr(void* allocatee_ptr) override
    {
        if (allocatee_ptr == nullptr)
        {
            return;
        }

        auto* typed_ptr = static_cast<SampleAllocateePtr<T>*>(allocatee_ptr);
        typed_ptr->~SampleAllocateePtr<T>();
    }

    void* GetAllocateeDataPtr(void* allocatee_ptr) override
    {
        if (allocatee_ptr == nullptr)
        {
            return nullptr;
        }

        auto* typed_ptr = static_cast<SampleAllocateePtr<T>*>(allocatee_ptr);
        return typed_ptr->Get();
    }

    bool SkeletonSendEventAllocatee(SkeletonEventBase* event_ptr, void* allocatee_ptr) override
    {
        if (event_ptr == nullptr || allocatee_ptr == nullptr)
        {
            return false;
        }
        auto skeleton_event = dynamic_cast<SkeletonEvent<T>*>(event_ptr);
        auto* typed_ptr = static_cast<SampleAllocateePtr<T>*>(allocatee_ptr);

        if (skeleton_event == nullptr || typed_ptr == nullptr)
        {
            return false;
        }
        return skeleton_event->Send(std::move(*typed_ptr)).has_value();
    }
};

/// \brief Interface for type-erased access to event, method, and field members
/// \details Provides virtual methods to retrieve ProxyEvent and SkeletonEvent members from their base pointers.
class MemberOperation
{
  public:
    virtual ~MemberOperation() = default;

    /// \brief Get ProxyEvent member from ProxyBase pointer
    /// \param proxy_ptr Pointer to ProxyBase instance
    /// \return Pointer to ProxyEventBase if found, nullptr otherwise
    virtual ProxyEventBase* GetProxyEvent(ProxyBase* proxy_ptr) = 0;

    /// \brief Get SkeletonEvent member from SkeletonBase pointer
    /// \param skeleton_ptr Pointer to SkeletonBase instance
    /// \return Pointer to SkeletonEventBase if found, nullptr otherwise
    virtual SkeletonEventBase* GetSkeletonEvent(SkeletonBase* skeleton_ptr) = 0;
};

/// \brief Template implementation of MemberOperation for specific ProxyType and SkeletonType
/// \details Provides concrete implementations to retrieve and cast ProxyEvent and SkeletonEvent members.
/// Template parameters specify the proxy type, skeleton type, event data type, and member pointers.
template <typename ProxyType,
          typename SkeletonType,
          typename EventType,
          auto proxy_event_member,
          auto skeleton_event_member>
class MemberOperationImpl : public MemberOperation
{
  public:
    ProxyEventBase* GetProxyEvent(ProxyBase* proxy_ptr) override
    {
        auto proxy = dynamic_cast<ProxyType*>(proxy_ptr);
        if (proxy == nullptr)
        {
            return nullptr;
        }

        return static_cast<ProxyEventBase*>(&(proxy->*proxy_event_member));
    }

    SkeletonEventBase* GetSkeletonEvent(SkeletonBase* skeleton_ptr) override
    {
        if (skeleton_ptr == nullptr)
        {
            return nullptr;
        }

        auto* skeleton = dynamic_cast<SkeletonType*>(skeleton_ptr);
        if (skeleton == nullptr)
        {
            return nullptr;
        }

        return static_cast<SkeletonEventBase*>(&(skeleton->*skeleton_event_member));
    }
};

/// \brief Interface for type-erased proxy and skeleton creation operations
/// \details Provides methods to create proxy/skeleton instances and register/retrieve member operations.
/// Maintains a registry of member operations for events, methods, and fields.
class InterfaceOperations
{
  public:
    virtual ~InterfaceOperations() = default;

    /// \brief Create a Proxy instance from a HandleType
    /// \param handle_ptr Opaque handle identifying the service instance
    /// \return Pointer to ProxyBase instance, or nullptr on failure
    virtual ProxyBase* CreateProxy(const HandleType& handle_ptr) = 0;

    /// \brief Create a Skeleton instance from an InstanceSpecifier
    /// \param instance_specifier InstanceSpecifier identifying the service to offer
    /// \return Pointer to SkeletonBase instance, or nullptr on failure
    virtual SkeletonBase* CreateSkeleton(const ::score::mw::com::InstanceSpecifier& instance_specifier) = 0;

    /// \brief Register member operation for event, method, or field
    /// \details Currently used for events. Stores operation in member operation map by name.
    /// \param member_name Name of the member (event/method/field) used as registry key
    /// \param ops Shared pointer to MemberOperation implementation
    void RegisterMemberOperation(const std::string_view member_name, std::shared_ptr<MemberOperation> ops)
    {
        member_operation_map_[member_name] = ops;
    }

    /// \brief Get member operation for event, method, or field
    /// \details Currently used for events. Retrieves operation from member operation map by name.
    /// \param member_name Name of the member (event/method/field) used as registry key
    /// \return Pointer to MemberOperation implementation if found, nullptr otherwise
    MemberOperation* GetMemberOperation(const std::string_view member_name)
    {
        auto it = member_operation_map_.find(member_name);
        if (it != member_operation_map_.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

  private:
    using MemberOperationMap = std::unordered_map<std::string_view, std::shared_ptr<MemberOperation>>;
    MemberOperationMap member_operation_map_;
};

/// \brief Template implementation of InterfaceOperations for specific Proxy and Skeleton types
/// \details Provides concrete implementations of CreateProxy and CreateSkeleton methods.
/// Instantiated and registered for each interface via macros.
template <typename AsProxy, typename AsSkeleton>
class InterfaceOperationImpl : public InterfaceOperations
{
  public:
    ProxyBase* CreateProxy(const HandleType& handle_ptr) override
    {
        // Pass the dereferenced reference directly
        auto result = AsProxy::Create(handle_ptr);

        if (result.has_value())
        {
            AsProxy* proxy = new AsProxy{std::move(result).value()};
            return proxy;
        }
        else
        {
            return nullptr;
        }
        // TODO: Need to think about exception handling here - Ticket-219876
    }

    SkeletonBase* CreateSkeleton(const ::score::mw::com::InstanceSpecifier& instance_specifier) override
    {
        if (auto result = AsSkeleton::Create(instance_specifier); result.has_value())
        {
            return (new AsSkeleton{std::move(result).value()});
        }
        else
        {
            return nullptr;
        }
    }
};

/// \brief Global registry for interface operations and type operations
/// \details Maintains static registries for interface operations (keyed by interface ID) and type operations
/// (keyed by type name). Used by macros to register and by FFI layer to find operations.
class GlobalRegistryMapping
{
  public:
    using InterfaceOprationMap = std::unordered_map<std::string_view, std::shared_ptr<InterfaceOperations>>;

    using TypeOperationMap = std::unordered_map<std::string_view, std::shared_ptr<TypeOperations>>;

    /// \brief Get the type operation map
    /// \details Creates static map on first call and returns reference to it for subsequent calls.
    /// \return Reference to the static type operation map
    static TypeOperationMap& GetTypeOperationMap()
    {
        static TypeOperationMap s_type_operation_map;
        return s_type_operation_map;
    }

    /// \brief Register type operation for a specific type name
    /// \details Called by EXPORT_MW_COM_TYPE macro to register type operations.
    /// \param type_name Name of the type used as key in registry
    /// \param impl Shared pointer to TypeOperations implementation
    static void RegisterTypeOperation(const std::string_view type_name, std::shared_ptr<TypeOperations> impl)
    {
        GetTypeOperationMap()[type_name] = std::move(impl);
    }

    /// \brief Get the interface operation map
    /// \details Creates static map on first call and returns reference to it for subsequent calls.
    /// \return Reference to the static interface operation map
    static InterfaceOprationMap& GetInterfaceOprationMap()
    {
        static InterfaceOprationMap s_factories;
        return s_factories;
    }

    /// \brief Register member operation for a specific interface and member name
    /// \details Called by EXPORT_MW_COM_EVENT macro to register member operations.
    /// \param interface_id ID of the interface used to locate the interface operation
    /// \param member_name Name of the member (event/method/field) used as registry key
    /// \param ops Shared pointer to MemberOperation implementation
    static void RegisterMemberOperation(const std::string_view interface_id,
                                        const std::string_view member_name,
                                        std::shared_ptr<MemberOperation> ops)
    {
        const auto& registries = GetInterfaceOperation(interface_id);
        if (registries)
        {
            registries->RegisterMemberOperation(member_name, std::move(ops));
        }
    }

    /// \brief Register interface operation for a specific interface ID
    /// \details Called by EXPORT_MW_COM_INTERFACE macro to register interface operations.
    /// \param interface_id ID of the interface used as registry key
    /// \param ops Shared pointer to InterfaceOperations implementation
    static void RegisterInterfaceOperation(const std::string_view interface_id,
                                           std::shared_ptr<InterfaceOperations> ops)
    {
        GetInterfaceOprationMap()[interface_id] = std::move(ops);
    }

    /// \brief Get interface operation for a specific interface ID
    /// \param interface_id ID of the interface used as registry key
    /// \return Pointer to InterfaceOperations implementation if found, nullptr otherwise
    static InterfaceOperations* GetInterfaceOperation(const std::string_view interface_id)
    {
        auto it = GetInterfaceOprationMap().find(interface_id);
        if (it != GetInterfaceOprationMap().end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    /// \brief Get type operation for a specific type name
    /// \param type_name Name of the type used as registry key
    /// \return Pointer to TypeOperations implementation if found, nullptr otherwise
    static TypeOperations* FindTypeInformation(const std::string_view type_name)
    {
        auto& type_ops_map = GetTypeOperationMap();
        auto it = type_ops_map.find(type_name);
        if (it != type_ops_map.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    /// \brief Get member operation for a specific interface and member name
    /// \param interface_id ID of the interface used to locate the interface operation
    /// \param member_name Name of the member used as registry key
    /// \return Pointer to MemberOperation implementation if found, nullptr otherwise
    static MemberOperation* FindMemberOperation(const std::string_view interface_id, const std::string_view member_name)
    {
        auto* ops = GetInterfaceOperation(interface_id);
        if (ops)
        {
            return ops->GetMemberOperation(member_name);
        }
        return nullptr;
    }
    /// \brief Find interface operation registry for a specific interface ID
    /// \param interface_id ID of the interface used as registry key
    /// \return Pointer to InterfaceOperations implementation if found, nullptr otherwise
    static InterfaceOperations* FindInterfaceRegistry(const std::string_view interface_id)
    {
        return GetInterfaceOperation(interface_id);
    }
};

// Declare the FFI function for Rust closure invocation with type erasure.
// This function will be defined in the FFI implementation file and called by C++ code to invoke Rust closures with
// SamplePtr arguments.
extern "C" {
/// \brief Rust closure invocation for SamplePtr with type erasure
/// \details This function is called by C++ to invoke a Rust closure with a sample pointer.
/// \param boxed_fnmut Pointer to FatPtr representing the Rust FnMut closure
/// \param sample_ptr Pointer to SamplePtr<T> representing the sample data
void mw_com_impl_call_dyn_ref_fnmut_sample(const ::score::mw::com::impl::rust::FatPtr* boxed_fnmut, void* sample_ptr);
}

/// \brief Macro to begin registration of interface operations
/// \details Creates registry and type aliases for a specific interface. Uses a static struct to register
/// interface operations at startup/before main(). Declares the Rust FFI function for closure invocation.
/// \param id Interface TYPE ID that the Rust side uses to identify the interface
/// \param proxy_type Proxy class type for the interface
/// \param skeleton_type Skeleton class type for the interface
/// \note Example usage: BEGIN_EXPORT_MW_COM_INTERFACE(VehicleInterface, VehicleProxy, VehicleSkeleton)
#define BEGIN_EXPORT_MW_COM_INTERFACE(id, proxy_type, skeleton_type)                                                 \
    namespace id##_detail                                                                                            \
    {                                                                                                                \
        constexpr std::string_view id_interface = #id;                                                               \
        /* Type aliases for event macros to use */                                                                   \
        using ProxyType = proxy_type;                                                                                \
        using SkeletonType = skeleton_type;                                                                          \
                                                                                                                     \
        /* Registration helper struct - constructor runs at startup */                                               \
        struct id##_InterfaceRegistrationHelper                                                                      \
        {                                                                                                            \
            id##_InterfaceRegistrationHelper()                                                                       \
            {                                                                                                        \
                /*TODO: We will validate if we can use unique_ptr here - Ticket-219875 */                               \
                auto interface_event_ops =                                                                           \
                    std::make_shared<::score::mw::com::impl::rust::InterfaceOperationImpl<ProxyType, SkeletonType>>(); \
                                                                                                                     \
                /* Register interface factory for FFI layer */                                                       \
                ::score::mw::com::impl::rust::GlobalRegistryMapping::RegisterInterfaceOperation(id_interface,          \
                                                                                              interface_event_ops);  \
            }                                                                                                        \
        };                                                                                                           \
                                                                                                                     \
        /* Force instantiation at startup */                                                                         \
        static id##_InterfaceRegistrationHelper id##_interface_reg_instance;

/// \brief Macro to register event member operations
/// \details Creates registry for event member operations for a specific interface and event name.
/// Uses a static struct to register member operations at startup/before main().
/// \param event_type Data type of the event
/// \param event_member Event member name in Proxy and Skeleton classes
/// \note Example usage: EXPORT_MW_COM_EVENT(Tire, left_tire)
#define EXPORT_MW_COM_EVENT(event_type, event_member)                                                             \
    struct event_member##_EventRegistrationHelper                                                                 \
    {                                                                                                             \
        event_member##_EventRegistrationHelper()                                                                  \
        {                                                                                                         \
                                                                                                                  \
            /* TODO: We will validate if we can use unique_ptr here - Ticket-219875 */                               \
            auto event_info =                                                                                     \
                std::make_shared<::score::mw::com::impl::rust::MemberOperationImpl<ProxyType,                       \
                                                                                 SkeletonType,                    \
                                                                                 event_type,                      \
                                                                                 &ProxyType::event_member,        \
                                                                                 &SkeletonType::event_member>>(); \
                                                                                                                  \
            /* Register this event in the LOCAL interface registry (not global) */                                \
            ::score::mw::com::impl::rust::GlobalRegistryMapping::RegisterMemberOperation(                           \
                std::string_view(id_interface), std::string_view(#event_member), event_info);                     \
        }                                                                                                         \
    };                                                                                                            \
                                                                                                                  \
    /* Force instantiation at startup */                                                                          \
    static event_member##_EventRegistrationHelper event_member##_event_reg_instance;

#define END_EXPORT_MW_COM_INTERFACE() }  // namespace id##_detail

/// \brief Macro to register type operations
/// \details Creates registry for type operations for a specific type name. Specializes the
/// RustRefMutCallable template and uses a static struct to register type operations at startup/before main().
/// \param type_tag Type name tag used in macros as the registry key
/// \param type Actual C++ type for which operations are registered
/// \note Example usage: EXPORT_MW_COM_TYPE(TireType, Tire)
#define EXPORT_MW_COM_TYPE(type_tag, type)                                                                          \
    template <>                                                                                                     \
    class score::mw::com::impl::rust::RustRefMutCallable<void, ::score::mw::com::impl::SamplePtr<type>>                 \
    {                                                                                                               \
      public:                                                                                                       \
        static void invoke(::score::mw::com::impl::rust::FatPtr ptr_,                                                 \
                           ::score::mw::com::impl::SamplePtr<type> sample) noexcept                                   \
        {                                                                                                           \
            /* Wrap in placement-new and call the Rust FFI function for closure invocation */                       \
            alignas(                                                                                                \
                ::score::mw::com::impl::SamplePtr<type>) char storage[sizeof(::score::mw::com::impl::SamplePtr<type>)]; \
            auto* placement_sample = new (storage)::score::mw::com::impl::SamplePtr<type>(std::move(sample));         \
            ::score::mw::com::impl::rust::mw_com_impl_call_dyn_ref_fnmut_sample(&ptr_, placement_sample);             \
        }                                                                                                           \
        static void dispose(::score::mw::com::impl::rust::FatPtr) noexcept {}                                         \
    };                                                                                                              \
                                                                                                                    \
    struct type_tag##_TypeRegistrationHelper                                                                        \
    {                                                                                                               \
        type_tag##_TypeRegistrationHelper()                                                                         \
        { /* TODO: We will validate if we can use unique_ptr here - Ticket-219875 */                                   \
            auto type_ops = std::make_shared<::score::mw::com::impl::rust::TypeOperationImpl<type>>();                \
            ::score::mw::com::impl::rust::GlobalRegistryMapping::RegisterTypeOperation(#type_tag, type_ops);          \
        }                                                                                                           \
    };                                                                                                              \
                                                                                                                    \
    static type_tag##_TypeRegistrationHelper type_tag##_type_reg_instance;
}  // namespace score::mw::com::impl::rust

#endif  // SCORE_MW_COM_REGISTRY_BRIDGE_MACROS_H
