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
#ifndef SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_H
#define SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_H

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/lock_file.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"
#include "score/os/errno.h"
#include "score/os/fcntl.h"
#include "score/os/mman.h"
#include "score/os/stat.h"
#include "score/os/utils/interprocess/interprocess_mutex.h"

#include <score/expected.hpp>
#include <score/memory_resource.hpp>

#include <sys/types.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace score::memory::shared
{
namespace test
{
class SharedMemoryResourceTestAttorney;
}

// Violated Autosar rule A10-1-1 which says: "Class shall not be derived from more than one base class which is not an
// interface class". But technically current inheritance complies that rule, because base class is pure and another
// one intended to maintain "enable_shared_from_this" concept.
//  NOLINTNEXTLINE(fuchsia-multiple-inheritance): Inherited the pure class and enable_shared_from_this
class SharedMemoryResource : public ISharedMemoryResource, public std::enable_shared_from_this<SharedMemoryResource>
{
  public:
    ~SharedMemoryResource() override;
    /**
     * \brief The SharedMemoryResource shall not be copyable, since
     *        it holds a file descriptor. Any copy logic would destroy
     *        the single access logic for this descriptor.
     */
    SharedMemoryResource(const SharedMemoryResource&) = delete;
    SharedMemoryResource& operator=(const SharedMemoryResource&) = delete;
    SharedMemoryResource(SharedMemoryResource&& other) = delete;
    SharedMemoryResource& operator=(SharedMemoryResource&&) = delete;

    const MemoryResourceProxy* getMemoryResourceProxy() noexcept override;

    /**
     * @brief Get the start address of the memory region that this memory resource is managing
     * @return void* start address of memory resource (e.g. mmap result)
     */
    void* getBaseAddress() const noexcept override;

    /**
     * @brief Get the start address of the region available to a user of this SharedMemoryResource.
     * @detail The SharedMemoryResource stores a control block at the start of the memory region. This function will
     *         return the address after the control block to which the user can freely write.
     * @return void* start address of memory resource after control block
     */
    void* getUsableBaseAddress() const noexcept override;

    /**
     * @brief brief Get the number of bytes allocated by the user in the memory region.
     *        Does not include any house keeping data (such as a control block) allocated by the memory resource.
     * @return number of bytes already allocated by the user
     */
    std::size_t GetUserAllocatedBytes() const noexcept override;

    /**
     * @brief Determines whether memory resource should bypass bounds checking when calling
     * MemoryResourceRegistry::GetBoundsFromIdentifier with a memory identifier.
     * @return true if bounds checking should be performed on resource, otherwise, false.
     */
    bool IsOffsetPtrBoundsCheckBypassingEnabled() const noexcept override
    {
        return false;
    }

    /**
     * @brief Gets the path of the named shared memory region in the filesystem.
     * @details If this shared memory region was created using SharedMemoryFactory::Open, SharedMemoryFactory::Create or
     * SharedMemoryFactory::CreateOrOpen it is a named shared memory region because a file system path needs to be
     * given. getPath will return exactly the path that was given to the previously mentioned functions. If this shared
     * memory region was created using SharedMemoryFactory::CreateAnonymous it is an anonymous shared memory region. No
     * path was given at creation time, thus, getPath will return nullptr.
     * @return std::string* path of shared memory region in case of named shared memory region, nullptr in case of
     * anonymous shared memory region.
     */
    const std::string* getPath() const noexcept override;

    /**
     * @brief Gives the identifier of the shared-memory region.
     * @details For anonymous and named shared-memory resources the identifier differs. They are in the form of "id:
     * <id>" or `"file: <path>", respectively.
     * @returns A view of the identifier.
     */
    std::string_view GetIdentifier() const noexcept override;

    /**
     * @brief Gets the file descriptor of the shared-memory region in the filesystem.
     * @return FileDescriptor file descriptor of shared-memory region
     */
    FileDescriptor GetFileDescriptor() const noexcept override;

    /**
     * @brief Checks whether the shared-memory region is located in the typed memory or not.
     * @return bool is_shm_in_typed_memory_ which is true if shared-memory region is located in the typed
     * memory, false otherwise.
     *
     */
    bool IsShmInTypedMemory() const noexcept override;

  protected:
    /// \brief Constructor of the class SharedMemoryResource.
    /// \details Constructor should only be used by SharedMemoryResource::Create, SharedMemoryResource::Open or
    /// SharedMemoryResource::CreateOrOpen
    /// \param input_path path of the memory region: a string that describes a regular file path name that will be
    /// created under /dev/shmem.
    /// \param acl_factory a factory of IAccessControlList object that is applied on the created shared memory
    /// resource.
    /// \param typed_memory_ptr std::shared_ptr to TypedMemory object: When this ptr is not equal to nullptr, it is
    /// responsible of allocating the memory in the typed region, otherwise it is a nullptr by default and memory can be
    /// allocated in the system os.
    SharedMemoryResource(std::string input_path,
                         AccessControlListFactory acl_factory,
                         std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept;

    /// \brief Constructor of the class SharedMemoryResource.
    /// \details Constructor should only be used by SharedMemoryResource::CreateAnonymous
    /// \param shared_memory_resource_id an ECU wide unique identifier for the created memory region.
    /// \param acl_factory a factory of IAccessControlList object that is applied on the created shared memory
    /// resource.
    /// \param typed_memory_ptr std::shared_ptr to TypedMemory object: When this ptr is not equal to nullptr, it is
    /// responsible of allocating the memory in the typed region, otherwise it is a nullptr by default and memory can be
    /// allocated in the system os.
    SharedMemoryResource(std::uint64_t shared_memory_resource_id,
                         AccessControlListFactory acl_factory,
                         std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept;

  private:
    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used.".
    // The 'friend' class is employed to encapsulate non-public members.
    // This design choice protects end users from implementation details
    // and prevents incorrect usage. Friend classes provide controlled
    // access to private members, utilized internally, ensuring that
    // end users cannot access implementation specifics.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SharedMemoryFactoryImpl;
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class test::SharedMemoryResourceTestAttorney;

    /// \brief Returns the space in shared-memory, which the SharedMemoryResource needs itself for its
    /// book-keeping/control.
    /// \details SharedMemoryResource itself populates the start of its shared-memory object with a
    /// ControlBlock to keep track of free space/already allocated memory. So when a SharedMemoryResource gets created
    /// via the SharedMemoryFactory::Create() method, the caller is responsible for handing over the space to reserve
    /// for the SharedMemoryResource in total. So he needs to know, what size the SharedMemoryResource needs itself!
    /// This is the reason for the existence of this func.
    /// The function returns a worst case size: The ControlBlock itself needs a specific alignment and the start of user
    /// allocated data behind the control block starts also at an (worst case -> alignof(std::max_align_t)) aligned
    /// address. Since we don't specify the address in mmap() calls, the shared-memory object will be created at a page
    /// boundary anyhow. So we don't have to care for the alignment (add any potential padding) for the ControlBlock.
    /// \return returns the size the SharedMemoryResource needs itself (independent from ay user data later stored in
    ///         the SharedMemoryResource).
    static constexpr std::size_t GetNeededManagementSpace() noexcept
    {
        return CalculateAlignedSize(sizeof(ControlBlock), alignof(std::max_align_t));
    }

    /**
     * @brief Get the end address of the shared memory region that this SharedMemoryResource is managing
     * @return void* end address of memory resource
     */
    const void* getEndAddress() const noexcept override;

    void reserveSharedMemory() const noexcept;
    void ApplyPermissions(const UserPermissions&) noexcept;
    /// \brief Compensate any access-right changes the umask may have inflicted.
    /// \details After creation of shared-mem object its access rights might have to be adapted as during creation the
    ///        process umask might have interfered. Typically the umask is set to '002' -> masks out write-access by
    ///        others.
    /// \param target_rights  target access rights, we want to achieve.
    void CompensateUmask(const os::Stat::Mode target_rights) const noexcept;

    void mapMemoryIntoProcess() noexcept;
    /// \brief initializes the control block, which will be located directly at the start address of the
    ///        SharedMemoryResource (see GetBaseAddress()).
    /// \details It initializes the start member, which points at the location, from where the first (user) memory
    /// allocation within this SharedMemoryResource will take place. This will be directly after the control block
    /// itself at the first address behind the control block, which is maximum/worst-case aligned.
    void initializeControlBlock() noexcept;
    void waitUntilInitializedByOtherProcess() const noexcept;

    score::cpp::expected_blank<score::os::Error> waitForOtherProcessAndOpen() noexcept;

    void loadInternalsFromSharedMemory() noexcept;
    void initializeInternalsInSharedMemory() noexcept;
    void deinitalizeInternalsInSharedMemory() noexcept;

    // Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate()
    // function shall not be called implicitly".
    // Rationale: getSharedPtr() may throw an exception if the SharedMemoryResource was not previously shared by a
    // std::shared_ptr. We always use static factory methods (Create, Open, etc) which return
    // shared_ptr<SharedMemoryResource> to create a SharedMemoryResource so this exception will never be thrown.
    // coverity[autosar_cpp14_a15_5_3_violation]
    std::shared_ptr<SharedMemoryResource> getSharedPtr() noexcept
    {
        return shared_from_this();
    }

    uid_t getOwnerUid() const noexcept;

    static std::string GetLockFilePath(const std::string& input_path) noexcept;

    /// \brief Creates shared-mem-object under the path (path_).
    /// \param input_path path of the memory region: a string that describes a regular file path name that will be
    /// created under /dev/shmem.
    /// \param user_space_to_reserve the shm-object is created/mapped with the given size plus
    /// some additional bytes needed
    ///        for internal bookkeeping/management!
    /// \param initialize_callback callback to initialize the user portion of the shm-object
    ///        (after internal management space)
    /// \param permissions access permissions to be applied to the created shm-object.
    /// \param acl_factory a factory of IAccessControlList object that is applied on the created shared memory resource.
    /// \param typed_memory_ptr std::unique_ptr to TypedMemory object: When this ptr is not equal to nullptr, it is
    /// responsible of allocating the memory in the typed region, otherwise it is a nullptr by default and memory can be
    /// allocated in the system os.
    /// \return in case of error an score::os::Error is returned.
    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> Create(
        std::string input_path,
        const std::size_t user_space_to_reserve,
        InitializeCallback initialize_callback,
        const UserPermissions& permissions,
        AccessControlListFactory acl_factory,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr = nullptr) noexcept;

    /// \brief Creates anonymous shared-mem-object.
    /// \attention This implementation only works in QNX environment because typed memory is only implemented for QNX
    /// and anonymous shared memory allocation in system memory uses QNX specific shm_open() parameters. In non-QNX
    /// environment this function will return error ENOTSUP.
    /// \param shared_memory_resource_id an ECU wide unique identifier for the created memory region.
    /// \param user_space_to_reserve the shm-object is created/mapped with the given size plus
    /// some additional bytes needed
    ///        for internal bookkeeping/management!
    /// \param initialize_callback callback to initialize the user portion of the shm-object
    ///        (after internal management space)
    /// \param permissions access permissions to be applied to the created shm-object.
    /// \param acl_factory a factory of IAccessControlList object that is applied on the created shared memory resource.
    /// \param typed_memory_ptr std::unique_ptr to TypedMemory object: When this ptr is not equal to nullptr, it is
    /// responsible of allocating the memory in the typed region, otherwise it is a nullptr by default and memory can be
    /// allocated in the system os.
    /// \return in case of error an score::os::Error is returned.
    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> CreateAnonymous(
        std::uint64_t shared_memory_resource_id,
        const std::size_t user_space_to_reserve,
        InitializeCallback initialize_callback,
        const UserPermissions& permissions,
        AccessControlListFactory acl_factory,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept;

    /// \brief Creates shared-mem-object under the path (path_) if it not yet exists or opens it otherwise.
    /// \param input_path path of the memory region: a string that describes a regular file path name that will be
    /// created under /dev/shmem.
    /// \param user_space_to_reserve in case a create is done the shm-object is created/mapped with the given size plus
    ///        some additional bytes needed for internal bookkeeping/management! In case an open is done this parameter
    ///        is ignored and the shm-object gets mapped into address space with the size of the underlying shm-object
    ///        file!
    /// \param initialize_callback in case a create is done, callback to initialize the user portion of the shm-object
    ///        (after internal management space)
    /// \param permissions access permissions to be applied to the created shm-object.
    /// \param acl_factory a factory of IAccessControlList object that is applied on the created shared memory resource.
    /// \param typed_memory_ptr std::shared_ptr to TypedMemory object: When this ptr is not equal to nullptr, it is
    /// responsible of allocating the memory in the typed region, otherwise it is a nullptr by default and memory can be
    /// allocated in the system os.
    /// \return in case of error an score::os::Error is returned.
    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> CreateOrOpen(
        std::string input_path,
        const std::size_t user_space_to_reserve,
        InitializeCallback initialize_callback,
        const UserPermissions& permissions,
        AccessControlListFactory acl_factory,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept;

    /// \brief Opens shared-mem-object under the path (path_) and maps it into memory with the length of the underlying
    ///        shm-object file.
    /// \param input_path path of the memory region: a string that describes a regular file path name that will be
    /// created under /dev/shmem.
    /// \param is_read_write
    /// \param permissions access permissions to be applied to the created shm-object.
    /// \param acl_factory a factory of IAccessControlList object that is applied on the created shared memory resource.
    /// \param typed_memory_ptr std::unique_ptr to TypedMemory object: When this ptr is not equal to nullptr, it is
    /// responsible of allocating the memory in the typed region, otherwise it is a nullptr by default and memory can be
    /// allocated in the system os.
    /// \return in case of error an score::os::Error is returned.
    static score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> Open(
        std::string input_path,
        const bool is_read_write,
        AccessControlListFactory acl_factory,
        std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept;

    /// \brief Called by SharedMemoryResource::Create() after calling the constructor.
    /// \return in case of error an score::os::Error is returned.
    score::cpp::expected_blank<score::os::Error> CreateImpl(const std::size_t user_space_to_reserve,
                                                   const InitializeCallback initialize_callback,
                                                   const UserPermissions& permissions) noexcept;

    /// \brief Called by SharedMemoryResource::CreateOrOpen() after calling the constructor.
    /// \return in case of error an score::os::Error is returned.
    score::cpp::expected_blank<score::os::Error> CreateOrOpenImpl(const std::size_t user_space_to_reserve,
                                                         InitializeCallback initialize_callback,
                                                         const UserPermissions& permissions) noexcept;

    /// \brief Called by SharedMemoryResource::Open() after calling the constructor.
    /// \return in case of error an score::os::Error is returned.
    score::cpp::expected_blank<score::os::Error> OpenImpl(const bool is_read_write) noexcept;

    /// \brief UnlinkFilesystemEntry() should be used by the skeleton to unlink the shared memory region so that
    /// no process can open it anymore. This will not deallocate the shared memory region, that is done in
    /// ~SharedMemoryResource() when the last process with the shared memory region open closes its file descriptor. If
    /// UnlinkFilesystemEntry() is not called, then the shared memory region will never be closed.
    void UnlinkFilesystemEntry() const noexcept override;

    static ::score::os::Stat::Mode calcStatModeForPermissions(const UserPermissions& permissions) noexcept;

    class ControlBlock
    {
      public:
        explicit ControlBlock(const std::size_t id) noexcept : mutex{}, alreadyAllocatedBytes{}, memoryResourceProxy{id}
        {
        }

        // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
        // be private.".
        // Rationale: There are no class invariants to maintain which could be violated by directly accessing these
        // member variables.
        // coverity[autosar_cpp14_m11_0_1_violation]
        score::os::InterprocessMutex mutex;
        // coverity[autosar_cpp14_m11_0_1_violation]
        std::atomic<std::size_t> alreadyAllocatedBytes;
        // coverity[autosar_cpp14_m11_0_1_violation]
        MemoryResourceProxy memoryResourceProxy;
    };

    FileDescriptor file_descriptor_;
    uid_t file_owner_uid_;
    std::optional<std::string> lock_file_path_;
    std::size_t virtual_address_space_to_reserve_;
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr_;
    ::score::os::Fcntl::Open opening_mode_;
    ::score::os::Mman::Protection map_mode_;
    void* base_address_;
    ControlBlock* control_block_;
    AccessControlListFactory acl_factory_;
    bool is_shm_in_typed_memory_;
    // Will contain `"file: " + path` in case of named shared memory resource
    // Will contain `"id: " + std::to_str(shared_memory_resource_id)` in case of anonymous shared memory resource
    std::string log_identification_;
    // Will contain `hash(path)` in case of named shared memory resource
    // Will contain shared_memory_resource_id in case of anonymous shared memory resource
    std::uint64_t memory_identifier_;
    // std::variant will contain the path (std::string) in case of named shared memory resource
    // std::variant will contain shared_memory_resource_id (std::uint64_t) in case of anonymous shared memory resource
    std::variant<std::string, std::uint64_t> shared_memory_resource_identifier_;
    void* start_;

    void* do_allocate(const std::size_t bytes, const std::size_t alignment) override;
    void do_deallocate(void*, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const memory_resource& other) const noexcept override;

    score::cpp::expected_blank<score::os::Error> CreateLockFileForNamedSharedMemory(std::optional<LockFile>& lock_file) noexcept;
    void AllocateInTypedMemory(const UserPermissions& permissions, os::Fcntl::Open& flags) noexcept;

    /// \brief Open shared memory from CreateImpl context
    /// \details This method is meant to be called exclusively from SharedMemoryResource::CreateImpl. It behaves
    ///          differently depending on the following use-cases:
    ///          - named shared memory in system memory: A shared memory object is created and opened. The flags
    ///            parameter must contain `Fcntl::Open::kCreate`
    ///          - named shared memory in typed memory: A shared memory object (previously created by a call to
    ///            SharedMemoryResource::AllocateInTypedMemory) is opened. The flags parameter must not contain
    ///            `Fcntl::Open::kCreate`
    ///          - anonymous shared memory in system memory: A shared memory object is created and opened. The flags
    ///            parameter is ignored
    ///          - anonymous shared memory in typed memory: This method essentially does nothing because the shared
    ///            memory is already opened by the call to AllocateAndOpenAnonymousTypedMemory inside
    ///            SharedMemoryResource::AllocateInTypedMemory method. The reason for this is that typed memory client
    ///            API returns a shm_handle_t which is only available on QNX. Thus, allocating and opening both are
    ///            encapsulated in TypedMemoryImpl::AllocateAndOpenAnonymousTypedMemory to keep SharedMemoryResource
    ///            class clean from "#if defined __QNX__". The flags parameter is ignored
    /// \param flags Fcntl::Open flags used for creating / opening the shared memory. See details section for further
    ///              explanation
    /// \param mode Stat::Mode used for creating / opening the shared memory
    /// \todo This method needs refactoring
    /// \return Empty result in case of success, score::os::Error in case of error.
    score::cpp::expected_blank<score::os::Error> OpenSharedMemory(const os::Fcntl::Open& flags, os::Stat::Mode mode) noexcept;
    void SealAnonymousOrReserveNamedSharedMemory() noexcept;
};

namespace detail
{

/// \brief Implementation of a simplistic monotonic allocation algo as used by do_allocate().
/// \param alloc_start address where allocation can start (start of free buffer space)
/// \param alloc_end address where allocation shall end (end of free buffer space)
/// \param bytes how many bytes to allocate
/// \param alignment
/// \return If allocation is successful a valid pointer is returned, otherwise a nullptr
void* do_allocation_algorithm(const void* const alloc_start,
                              const void* const alloc_end,
                              const std::size_t bytes,
                              const std::size_t alignment) noexcept;

}  // namespace detail

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SHARED_MEMORY_RESOURCE_H
