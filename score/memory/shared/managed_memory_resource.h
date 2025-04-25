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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_MANAGED_MEMORY_RESOURCE_H
#define BASELIBS_SCORE_MEMORY_SHARED_MANAGED_MEMORY_RESOURCE_H

#include <score/memory_resource.hpp>
#include <type_traits>

namespace score::memory::shared
{
namespace test
{
// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// The forward declaration of ManagedMemoryResourceTestAttorney is necessary to establish a friend relation with
// ManagedMemoryResource that facilitates easier testing.
// ManagedMemoryResourceTestAttorney is only declared once hence it is a false positive.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
class ManagedMemoryResourceTestAttorney;
}  // namespace test

// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// The forward declaration of MemoryResourceProxy is necessary to avoid cyclic dependencies and to ensure that the
// ManagedMemoryResource class can be declared without requiring the full definition of MemoryResourceProxy. This
// forward declaration does not violate the AUTOSAR C++14 M3-2-3 guideline as it is a common practice to handle
// dependencies in large codebases. The full definition of MemoryResourceProxy will be provided elsewhere in the
// codebase.
// coverity[autosar_cpp14_m3_2_3_violation]
class MemoryResourceProxy;

/**
 * \brief The ManagedMemoryResource extends the C++17 defined
 *        std::pmr::memory_resource by an interface to retrieve an so-called
 *        MemoryResourceProxy.
 *        This MemoryResourceProxy can then be shared (e.g. over shared memory)
 *        and be used to identify a ManagedMemoryResource to allocate memory. This is always then necessary
 *        when the underlying memory_resource cannot be shared.
 *
 *        In our particular case the std::pmr::memory_resource cannot be shared over shared memory,
 *        since it include a v-table which is non-valid over process boundaries.
 */

// Suppress "AUTOSAR C++14 M3-2-3" rule finding: "A type, object or function that is used in multiple translation units
// shall be declared in one and only one file.".
// this is false positive. ManagedMemoryResource is declared only once.
// coverity[autosar_cpp14_m3_2_3_violation : FALSE]
class ManagedMemoryResource : public ::score::cpp::pmr::memory_resource
{
  public:
    ManagedMemoryResource() noexcept = default;
    ~ManagedMemoryResource() noexcept override = default;

    /**
     * We need to return a raw pointer, since we need to convert this
     * pointer into an OffsetPtr if it shall be stored in shared memory.
     * @return MemoryResourceProxy* that identifies _this_ memory_resource.
     */

    /// \todo: getMemoryResourceProxy should not return a non const pointer and the method should also be marked const.
    /// This issue will be investigated and fixed in Ticket-146625"
    virtual const MemoryResourceProxy* getMemoryResourceProxy() noexcept = 0;

    /**
     * @brief Construct T allocating underlying MemoryResource
     * @tparam T The type that shall be constructed
     * @tparam Args The argument types
     * @param args The argument values to construct T
     * @return T* The pointer to the constructed data type
     */
    template <typename T, typename... Args>
    T* construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        void* const memory = this->allocate(sizeof(T), alignof(T));
        // Operator \c new doesn't allocate any new resources, instead of that preallocated buffer is
        // NOLINTNEXTLINE(score-no-dynamic-raw-memory): used by placement new
        return new (memory) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Destruct and deallocate T* in underlying MemoryResource
     * @tparam T The type that shall be destructed
     * @param t The actual T instance that shall be destructed
     */
    template <typename T, typename = std::enable_if_t<!std::is_pointer_v<T>>>
    void destruct(T& t)
    {
        t.~T();
        this->deallocate(&t, sizeof(T));
    }

    /**
     * @brief Get the start address of the memory region that this memory resource is managing
     * @return void* start address of memory resource (e.g. mmap result)
     */
    virtual void* getBaseAddress() const noexcept = 0;

    /**
     * @brief Get the start address of the region available to a user of this memory resource.
     *        The memory resource may store some house keeping data (such as a control block) at the start of the memory
     *        region. This function will return the address after that to which the user can freely write.
     * @return void* start address of memory resource after house keeping data
     */
    virtual void* getUsableBaseAddress() const noexcept = 0;

    /**
     * @brief brief Get the number of bytes allocated by the user in the memory region.
     *        Does not include any house keeping data (such as a control block) allocated by the memory resource.
     * @return number of bytes already allocated by the user
     */
    virtual std::size_t GetUserAllocatedBytes() const noexcept = 0;

    /**
     * @brief Determines whether memory resource should bypass bounds checking when calling
     * MemoryResourceRegistry::GetBoundsFromIdentifier with a memory identifier.
     * @return false if bounds checking should be performed on the resource, otherwise, true.
     */
    virtual bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept
    {
        return false;
    }

  protected:
    ManagedMemoryResource(const ManagedMemoryResource&) noexcept = default;
    ManagedMemoryResource(ManagedMemoryResource&&) noexcept = default;
    ManagedMemoryResource& operator=(const ManagedMemoryResource&) noexcept = default;
    ManagedMemoryResource& operator=(ManagedMemoryResource&&) noexcept = default;

  private:
    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used.".
    // The 'friend' class is employed to encapsulate non-public members. This design choice protects end users from
    // implementation details and prevents incorrect usage. Friend classes provide controlled access to private members,
    // utilized internally, ensuring that end users cannot access implementation specifics.
    // This is for testing only
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class test::ManagedMemoryResourceTestAttorney;
    // We make MemoryResourceRegistry a friend since it needs to access private internals of ManagedMemoryResource which
    // we do not want to expose to the user via the public interface of ManagedMemoryResource.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class MemoryResourceRegistry;

    /**
     * @brief Get the end address of the memory region that this memory resource is managing
     * @details Like with iterators, this returns a a past-the-end-address. E.g. the first byte, which
     *          isn't usable anymore/lies after the valid memory region of this resource.
     * @return void* past-the-end address of memory resource
     */
    virtual const void* getEndAddress() const noexcept = 0;
};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_MANAGEDMEMORYRESOURCE_H
