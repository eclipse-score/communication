// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#include "score/containers/dynamic_array.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/shared_memory_resource.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace
{

template <typename T>
using shm_dynamic_array = score::containers::DynamicArray<T, score::memory::shared::PolymorphicOffsetPtrAllocator<T>>;

constexpr std::size_t kNumberOfElements{50};
constexpr std::chrono::milliseconds kPollingInterval{100};
constexpr std::size_t kSharedMemorySize{65536};

struct MemoryLayout
{
    explicit MemoryLayout(score::memory::shared::ManagedMemoryResource& resource)
        : dynamic_array{kNumberOfElements, resource}
    {
    }

    std::atomic_bool client_exists{false};
    shm_dynamic_array<std::uint32_t> dynamic_array;
};

class SharedMemoryResourceGuard
{
  public:
    SharedMemoryResourceGuard(score::memory::shared::SharedMemoryFactory::InitializeCallback callback,
                              std::string memory_path) noexcept
        : resource_{score::memory::shared::SharedMemoryFactory::CreateOrOpen(memory_path,
                                                                             std::move(callback),
                                                                             kSharedMemorySize)},
          memory_path_{std::move(memory_path)}
    {
    }

    ~SharedMemoryResourceGuard() noexcept
    {
        score::memory::shared::SharedMemoryFactory::Remove(memory_path_);
    }

    SharedMemoryResourceGuard(const SharedMemoryResourceGuard&) = delete;
    SharedMemoryResourceGuard& operator=(const SharedMemoryResourceGuard&) = delete;
    SharedMemoryResourceGuard(SharedMemoryResourceGuard&&) = delete;
    SharedMemoryResourceGuard& operator=(SharedMemoryResourceGuard&&) = delete;

    std::shared_ptr<score::memory::shared::ISharedMemoryResource> GetResource() const noexcept
    {
        return resource_;
    }

  private:
    std::shared_ptr<score::memory::shared::ISharedMemoryResource> resource_;
    const std::string memory_path_;
};

}  // namespace

/**
 * \brief   Small test program to test, whether DynamicArray with a PolymorphicOffsetAllocator works in shared memory
 *
 * \details Two processes are started, both running this main. Both try to CreateOrOpen the same shared-mem-object.
 *          The one, who comes 1st initializes the shared-memory and the DynamicArray within it and writes a given
 *          pattern to it. The one, who comes 2nd then reads out from DynamicArray and verifies the data.
 *
 * \return -1 on error, 0 on success
 */
int main(int /*argc*/, char* /*argv*/[])
{
    MemoryLayout* ptr{nullptr};
    auto callback = [&ptr](const std::shared_ptr<score::memory::shared::ISharedMemoryResource>& resource) {
        ptr = resource->construct<MemoryLayout>(*resource);
        std::cout << "Producer: Size of DynamicArray:" << ptr->dynamic_array.size() << std::endl;
        for (std::uint32_t i = 0; i < ptr->dynamic_array.size(); i++)
        {
            ptr->dynamic_array.at(i) = i;
        }
    };

    SharedMemoryResourceGuard resource_guard(std::move(callback), "/testing_shared_memory");
    if (ptr == nullptr)
    {
        // This is the branch for the 2nd / slower process.
        if (!resource_guard.GetResource())
        {
            std::cout << "Consumer: Could not get SharedMemoryResource!" << std::endl;
            return EXIT_FAILURE;
        }
        ptr = static_cast<MemoryLayout*>(resource_guard.GetResource()->getUsableBaseAddress());
        // signal to provider, that we have opened/mapped shm-object
        ptr->client_exists = true;
        std::cout << "Consumer: Size of DynamicArray:" << ptr->dynamic_array.size() << std::endl;
        for (std::uint32_t i = 0; i < ptr->dynamic_array.size(); i++)
        {
            if (ptr->dynamic_array.at(i) != i)
            {
                std::cout << "Element: " << i << " of DynamicArray has unexpected value: " << ptr->dynamic_array.at(i)
                          << std::endl;
                return EXIT_FAILURE;
            }
        }
    }
    else
    {
        // This is the branch for the 1st / quicker process, which succeeded in shm-object creation.
        // give the 2nd some time to open shm-object, before we go down and unlink shm-object.
        std::cout << "Producer: waiting for client/consumer." << std::endl;
        while (!ptr->client_exists)
        {
            std::this_thread::sleep_for(kPollingInterval);
        }
    }

    std::cout << "Success." << std::endl;
    return EXIT_SUCCESS;
}
