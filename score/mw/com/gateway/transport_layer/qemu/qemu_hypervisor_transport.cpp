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
#include "score/mw/com/gateway/transport_layer/qemu/qemu_hypervisor_transport.h"

#include "score/mw/com/gateway/transport_layer/sample/messages/gateway_messages.h"
#include "score/mw/log/logging.h"

#include "score/memory/shared/shared_memory_factory.h"

#include <cstdint>
#include <string>

#if defined(__QNXNTO__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace score::mw::com::gateway
{

ShmPaths ResolveInterVmShmPaths(const impl::InstanceSpecifier& specifier)
{
    // Constructs the inter-VM shm paths using the well-known prefix that
    // SharedMemoryFactory routes through the InterVM memory provider.
    //
    // In the full LoLa production stack, the skeleton uses ShmPathBuilder(service_id, inter_vm=true)
    // to create paths like: /intervm-shared-shmem/lola-data-XXXX-YYYY
    // The ShmPathBuilder is an internal LoLa binding detail (visibility restricted to
    // impl/plumbing). The transport layer cannot access it directly.
    //
    // In production integration, the GatewayCore would resolve the specifier via Runtime,
    // extract the LoLa deployment info (service_id + instance_id), and pass the resolved
    // paths to the transport — either via an extended ProvideService API or by storing them
    // in a transport-accessible registry. For now, we construct paths from the specifier
    // string, which works for the integration test where paths are self-consistent.
    // TODO: The path construction below is a temporary placeholder for integration testing.
    // In production, GatewayCore must resolve the specifier via Runtime, extract the LoLa
    // deployment info (service_id + instance_id), and supply the canonical ShmPathBuilder
    // paths to the transport. Track in issue #TBD.
    const std::string base = std::string{"/intervm-shared-shmem/"} + std::string{specifier.ToString()};
    return ShmPaths{base + "/ctrl", base + "/data"};
}

#if defined(__QNXNTO__)
ShmSizes GetInterVmShmSizes(const impl::InstanceSpecifier& specifier, const score::os::qnx::MmanQnx* mman_qnx)
{
    // Query the actual sizes of the existing shm objects that the local skeleton created.
    // The source-side skeleton has already called SharedMemoryFactory::Create() for both
    // CTRL and DATA shm. We open them briefly to read their size via fstat().
    const auto paths = ResolveInterVmShmPaths(specifier);
    ShmSizes sizes{0U, 0U};

    // Query CTRL shm size
    {
        std::int32_t fd = -1;
        const auto fd_result = mman_qnx->shm_open(paths.control.c_str(), O_RDONLY, 0);
        if (fd_result.has_value())
        {
            fd = fd_result.value();
        }
        if (fd != -1)
        {
            struct stat st{};
            if (::fstat(fd, &st) == 0)
            {
                sizes.control = static_cast<std::uint32_t>(st.st_size);
            }
            (void)::close(fd);
        }
    }
    // Query DATA shm size
    {
        std::int32_t fd = -1;
        const auto fd_result = mman_qnx->shm_open(paths.data.c_str(), O_RDONLY, 0);
        if (fd_result.has_value())
        {
            fd = fd_result.value();
        }
        if (fd != -1)
        {
            struct stat st{};
            if (::fstat(fd, &st) == 0)
            {
                sizes.data = static_cast<std::uint32_t>(st.st_size);
            }
            (void)::close(fd);
        }
    }
    return sizes;
}
#else
ShmSizes GetInterVmShmSizes([[maybe_unused]] const impl::InstanceSpecifier& specifier)
{
    return ShmSizes{0U, 0U};
}
#endif

QemuHypervisorTransport::QemuHypervisorTransport(
    GatewayCore& gateway_app,
    std::unique_ptr<IBidirectionalTransport> transport,
    std::shared_ptr<qemu::ivshmem::IvshmemTypedMemoryProvider> ivshmem_provider,
    ShmPathResolver path_resolver) noexcept
    : gateway_app_{gateway_app},
      message_transport_{std::move(transport)},
      ivshmem_provider_{std::move(ivshmem_provider)},
      path_resolver_{std::move(path_resolver)}
{
}

QemuHypervisorTransport::~QemuHypervisorTransport()
{
    Shutdown();
}

bool QemuHypervisorTransport::IsMemorySharingSupported() const
{
    return true;
}

score::ResultBlank QemuHypervisorTransport::Setup()
{
    message_transport_->SetMessageHandler([this](std::unique_ptr<TransportMessage> message) {
        OnMessageReceived(std::move(message));
    });
    return message_transport_->Setup();
}

void QemuHypervisorTransport::OnMessageReceived(std::unique_ptr<TransportMessage> message)
{
    if (!message)
    {
        log::LogError("LoLa") << "QemuTransport: Invalid message received: nullptr";
        return;
    }

    if (message->GetType() == MessageType::kProvideServiceRequest)
    {
        HandleProvideServiceRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kStopOfferServiceRequest)
    {
        HandleStopOfferServiceRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kOfferServiceRequest)
    {
        HandleOfferServiceRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kRegisterNotificationRequest)
    {
        HandleRegisterNotificationRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kUnregisterNotificationRequest)
    {
        HandleUnregisterNotificationRequest(std::move(message));
    }
    else if (message->GetType() == MessageType::kUpdateNotification)
    {
        HandleUpdateNotification(std::move(message));
    }
    else
    {
        log::LogError("LoLa") << "QemuTransport: Unexpected TransportMessage received: "
                              << static_cast<int>(message->GetType());
    }
}

void QemuHypervisorTransport::HandleProvideServiceRequest(std::unique_ptr<TransportMessage> message)
{
    auto* const request_ptr = dynamic_cast<ProvideServiceRequest*>(message.get());
    if (request_ptr == nullptr)
    {
        log::LogError("LoLa") << "QemuTransport: message type mismatch in HandleProvideServiceRequest";
        return;
    }
    auto& request = *request_ptr;
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "QemuTransport: Invalid instance specifier in ProvideServiceRequest!";
        return;
    }
    PreCreateInterVmSharedMemory(specifier_result.value(), request.GetShmControlSize(), request.GetShmDataSize());
    gateway_app_.ProvideService(specifier_result.value(), request.GetServiceElements());
}

void QemuHypervisorTransport::HandleStopOfferServiceRequest(std::unique_ptr<TransportMessage> message)
{
    auto* const request_ptr = dynamic_cast<StopOfferServiceRequest*>(message.get());
    if (request_ptr == nullptr)
    {
        log::LogError("LoLa") << "QemuTransport: message type mismatch in HandleStopOfferServiceRequest";
        return;
    }
    auto& request = *request_ptr;
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "QemuTransport: Invalid instance specifier in StopOfferServiceRequest!";
        return;
    }
    gateway_app_.StopOfferService(specifier_result.value());
}

void QemuHypervisorTransport::HandleOfferServiceRequest(std::unique_ptr<TransportMessage> message)
{
    auto* const request_ptr = dynamic_cast<OfferServiceRequest*>(message.get());
    if (request_ptr == nullptr)
    {
        log::LogError("LoLa") << "QemuTransport: message type mismatch in HandleOfferServiceRequest";
        return;
    }
    auto& request = *request_ptr;
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "QemuTransport: Invalid instance specifier in OfferServiceRequest!";
        return;
    }
    gateway_app_.OfferService(specifier_result.value());
}

void QemuHypervisorTransport::HandleUpdateNotification(std::unique_ptr<TransportMessage> message)
{
    auto* const notification_ptr = dynamic_cast<UpdateNotification*>(message.get());
    if (notification_ptr == nullptr)
    {
        log::LogError("LoLa") << "QemuTransport: message type mismatch in HandleUpdateNotification";
        return;
    }
    auto& notification = *notification_ptr;
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{notification.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "QemuTransport: Invalid instance specifier in UpdateNotification!";
        return;
    }
    gateway_app_.NotifyUpdate(specifier_result.value(), notification.GetElementType(), notification.GetElementName());
}

void QemuHypervisorTransport::HandleRegisterNotificationRequest(std::unique_ptr<TransportMessage> message)
{
    auto* const request_ptr = dynamic_cast<RegisterNotificationRequest*>(message.get());
    if (request_ptr == nullptr)
    {
        log::LogError("LoLa") << "QemuTransport: message type mismatch in HandleRegisterNotificationRequest";
        return;
    }
    auto& request = *request_ptr;
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "QemuTransport: Invalid instance specifier in RegisterNotificationRequest!";
        return;
    }
    gateway_app_.RegisterUpdateNotification(
        specifier_result.value(), request.GetElementType(), request.GetElementName());
}

void QemuHypervisorTransport::HandleUnregisterNotificationRequest(std::unique_ptr<TransportMessage> message)
{
    auto* const request_ptr = dynamic_cast<UnregisterNotificationRequest*>(message.get());
    if (request_ptr == nullptr)
    {
        log::LogError("LoLa") << "QemuTransport: message type mismatch in HandleUnregisterNotificationRequest";
        return;
    }
    auto& request = *request_ptr;
    auto specifier_result = impl::InstanceSpecifier::Create(std::string{request.GetInstanceSpecifier()});
    if (!specifier_result.has_value())
    {
        log::LogError("LoLa") << "QemuTransport: Invalid instance specifier in UnregisterNotificationRequest!";
        return;
    }
    gateway_app_.UnregisterUpdateNotification(
        specifier_result.value(), request.GetElementType(), request.GetElementName());
}

void QemuHypervisorTransport::Shutdown()
{
    message_transport_->Shutdown();
}

score::ResultBlank QemuHypervisorTransport::ProvideService(impl::InstanceSpecifier service_instance_specifier,
                                                           std::vector<impl::EventInfo> service_elements)
{
#if defined(__QNXNTO__)
    const auto shm_sizes = GetInterVmShmSizes(service_instance_specifier, ivshmem_provider_->GetMmanQnx());
#else
    const auto shm_sizes = GetInterVmShmSizes(service_instance_specifier);
#endif

    ProvideServiceRequest request{
        std::move(service_instance_specifier), std::move(service_elements), shm_sizes.control, shm_sizes.data};
    return message_transport_->SendRequest(request);
}

score::ResultBlank QemuHypervisorTransport::OfferService(impl::InstanceSpecifier service_instance_specifier)
{
    OfferServiceRequest request{std::move(service_instance_specifier)};
    return message_transport_->SendRequest(request);
}

score::ResultBlank QemuHypervisorTransport::StopOfferService(impl::InstanceSpecifier service_instance_specifier)
{
    StopOfferServiceRequest request{std::move(service_instance_specifier)};
    return message_transport_->SendRequest(request);
}

score::ResultBlank QemuHypervisorTransport::NotifyUpdate(impl::InstanceSpecifier service_instance_specifier,
                                                         impl::ServiceElementType updated_element_type,
                                                         std::string updated_element_name)
{
    UpdateNotification notification{
        std::move(service_instance_specifier), updated_element_type, std::move(updated_element_name)};
    return message_transport_->SendNotification(notification);
}

score::ResultBlank QemuHypervisorTransport::RegisterUpdateNotification(
    impl::InstanceSpecifier service_instance_specifier,
    impl::ServiceElementType element_type,
    std::string element_name)
{
    RegisterNotificationRequest request{std::move(service_instance_specifier), element_type, std::move(element_name)};
    return message_transport_->SendRequest(request);
}

score::ResultBlank QemuHypervisorTransport::UnregisterUpdateNotification(
    impl::InstanceSpecifier service_instance_specifier,
    impl::ServiceElementType element_type,
    std::string element_name)
{
    UnregisterNotificationRequest request{std::move(service_instance_specifier), element_type, std::move(element_name)};
    return message_transport_->SendRequest(request);
}

void QemuHypervisorTransport::PreCreateInterVmSharedMemory(const impl::InstanceSpecifier& specifier,
                                                           std::uint32_t shm_control_size,
                                                           std::uint32_t shm_data_size)
{
    auto paths = path_resolver_(specifier);
    if (paths.control.empty())
    {
        ::score::mw::log::LogError() << "PreCreateInterVmSharedMemory: failed to resolve SHM paths for "
                                     << specifier.ToString();
        return;
    }

    // Look up the BAR offsets from the BAR-resident directory written by the source VM.
    // This ensures both VMs bind the same name to the same physical BAR sub-range.
    const auto ctrl_offset = ivshmem_provider_->LookupOffsetInDirectory(paths.control);
    const auto data_offset = ivshmem_provider_->LookupOffsetInDirectory(paths.data);

    if (ctrl_offset.has_value())
    {
        const auto ctrl_result = ivshmem_provider_->AllocateNamedTypedMemoryAtOffset(
            shm_control_size,
            paths.control,
            ctrl_offset.value(),
            score::memory::shared::SharedMemoryFactory::WorldWritable{});  // COV_JUSTIFIED
                                                                           // qemu-worldwritable-ctor-gcc-artifact
        if (!ctrl_result.has_value())
        {
            ::score::mw::log::LogError() << "PreCreateInterVmSharedMemory: failed to bind CTRL shm to BAR for "
                                         << specifier.ToString() << " at offset " << ctrl_offset.value();
            return;
        }
    }
    else if (shm_control_size > 0U)
    {
        ::score::mw::log::LogWarn() << "PreCreateInterVmSharedMemory: CTRL offset not found in directory for "
                                    << specifier.ToString() << ", skipping CTRL binding";
    }

    if (data_offset.has_value())
    {
        const auto data_result = ivshmem_provider_->AllocateNamedTypedMemoryAtOffset(
            shm_data_size,
            paths.data,
            data_offset.value(),
            score::memory::shared::SharedMemoryFactory::WorldWritable{});  // COV_JUSTIFIED
                                                                           // qemu-worldwritable-ctor-gcc-artifact
        if (!data_result.has_value())
        {
            ::score::mw::log::LogError() << "PreCreateInterVmSharedMemory: failed to bind DATA shm to BAR for "
                                         << specifier.ToString() << " at offset " << data_offset.value();
            return;
        }
    }
    else if (shm_data_size > 0U)
    {
        ::score::mw::log::LogWarn() << "PreCreateInterVmSharedMemory: DATA offset not found in directory for "
                                    << specifier.ToString() << ", skipping DATA binding";
    }

    ::score::mw::log::LogInfo() << "PreCreateInterVmSharedMemory: bound CTRL(" << paths.control << ", "
                                << shm_control_size
                                << "B, offset=" << (ctrl_offset.has_value() ? ctrl_offset.value() : 0U) << ") + DATA("
                                << paths.data << ", " << shm_data_size
                                << "B, offset=" << (data_offset.has_value() ? data_offset.value() : 0U)
                                << ") to ivshmem BAR for " << specifier.ToString();
}

}  // namespace score::mw::com::gateway
