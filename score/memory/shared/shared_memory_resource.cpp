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
#include "score/memory/shared/shared_memory_resource.h"
#include "score/language/safecpp/safe_math/details/addition_subtraction/addition_subtraction.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/pointer_arithmetic_util.h"
#include "score/memory/shared/sealedshm/sealedshm_wrapper/sealed_shm.h"
#include "score/memory/shared/typedshm/typedshm_wrapper/typed_memory.h"

#include "score/language/safecpp/safe_math/safe_math.h"
#include "score/bitmanipulation/bitmask_operators.h"
#include "score/os/errno.h"
#include "score/os/errno_logging.h"
#include "score/os/fcntl.h"
#include "score/os/mman.h"
#include "score/os/stat.h"
#include "score/os/unistd.h"
#include "score/os/utils/acl/i_access_control_list.h"
#include "score/mw/log/log_stream.h"
#include "score/mw/log/log_types.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/hash.hpp>
#include <score/move_only_function.hpp>
#include <score/string_view.hpp>
#include <score/utility.hpp>
#include <cerrno>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace score::memory::shared
{

namespace
{
using Error = ::score::os::Error;
using ::score::os::Fcntl;
using ::score::os::Mman;
using ::score::os::Stat;

// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Rationale: Pre-processor commands are used to allow different implementations for linux and QNX to exist in the same
// file. This keeps both implementations close (i.e. within the same functions) which makes the code easier to read and
// maintain. It also prevents compiler errors in linux code when compiling for QNX and vice versa.
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined(__QNX__)
// In QNX /tmp is just an "alias" for /dev/shmem/, but using the alias
// has a performance drawback! Therefore we use directly /dev/shmem/ (see Ticket-131757)
constexpr auto kTmpPathPrefix = "/dev/shmem";
// coverity[autosar_cpp14_a16_0_1_violation]
#else
constexpr auto kTmpPathPrefix = "/tmp";
// coverity[autosar_cpp14_a16_0_1_violation]
#endif

constexpr auto readWriteAccessForUser = Stat::Mode::kReadUser | Stat::Mode::kWriteUser;
constexpr auto readAccessForEveryBody = readWriteAccessForUser | Stat::Mode::kReadGroup | Stat::Mode::kReadOthers;
constexpr auto readWriteAccessForEveryBody =
    readAccessForEveryBody | Stat::Mode::kWriteGroup | Stat::Mode::kWriteOthers;

constexpr auto read_only = ::score::os::Stat::Mode::kReadUser;
constexpr score::os::IAccessControlList::UserIdentifier kTypedmemdUid = 3020;

/// \brief Class aggregating info about shm-object read out via stat/fstat from the shm-object file
struct ShmObjectStatInfo
{
    // Suppress "AUTOSAR C++14 A9-6-1" rule findings. This rule declares: "Data types used for interfacing with hardware
    // or conforming to communication protocols shall be trivial, standard-layout and only contain members of types with
    // defined sizes."
    // While true that uid_t is not a fixed width integer it is required by the POSIX standard here.
    // coverity[autosar_cpp14_a9_6_1_violation]
    uid_t owner_uid;
    std::size_t size;
    bool is_shm_in_typed_memory;
};

// Helper class which allows SharedMemoryResource::Create() to call protected SharedMemoryResource constructors.
class MakeSharedEnabler final : public SharedMemoryResource
{
  public:
    template <typename... Args>
    explicit MakeSharedEnabler(Args&&... args) : SharedMemoryResource(std::forward<Args>(args)...)
    {
    }

    ~MakeSharedEnabler() final = default;
    MakeSharedEnabler(const MakeSharedEnabler&) = delete;
    MakeSharedEnabler(MakeSharedEnabler&&) noexcept = delete;
    MakeSharedEnabler& operator=(const MakeSharedEnabler&) = delete;
    MakeSharedEnabler& operator=(MakeSharedEnabler&&) noexcept = delete;
};

template <typename... Args>
static std::shared_ptr<SharedMemoryResource> CreateInstance(Args&&... args)
{
    return std::make_shared<MakeSharedEnabler>(std::forward<Args>(args)...);
}

bool doesFileExist(const score::cpp::string_view filePath)
{
    ::score::os::StatBuffer buffer{};
    const auto result = ::score::os::Stat::instance().stat(filePath.data(), buffer);
    if (!result.has_value())
    {
        if (result.error() != Error::Code::kNoSuchFileOrDirectory)
        {
            // LCOV_EXCL_BR_START (Tool false positive: There is no decision decision to be made in the logging
            // statement)
            // Unexpected error, perform respective logging, but our decision does not change. File does not exist
            ::score::mw::log::LogError("shm")
                << "Querying attributes for file " << filePath << "failed with errno" << result.error();
            // LCOV_EXCL_BR_STOP
        }
    }

    return result.has_value();
}

bool waitForFreeLockFile(const std::string& lock_file_path)
{
    constexpr std::chrono::milliseconds timeout{500};
    constexpr std::chrono::milliseconds retryAfter{10};
    constexpr auto maxRetryCount = safe_math::Cast<std::size_t>(timeout / retryAfter).value();

    std::uint8_t retryCount{0U};
    static_assert(maxRetryCount <= std::numeric_limits<decltype(retryCount)>::max(),
                  "Counter `retryCount` cannot hold maxRetryCount.");

    bool lockFileExists = doesFileExist(lock_file_path.c_str());
    // LCOV_EXCL_BR_START (Tool false positive: We check check all decisions i.e. that lock file does not exist, lock
    // file exists and retry count is less than max retry count and lock file exists and retry count is equal to max
    // retry count. It's impossible that the retry count check will fail when entering the loop which may be what the
    // tool wants us to check.)
    while (lockFileExists && (retryCount < maxRetryCount))
    {
        lockFileExists = doesFileExist(lock_file_path.c_str());
        retryCount++;
        std::this_thread::sleep_for(retryAfter);
    }
    // LCOV_EXCL_BR_STOP
    return !lockFileExists;  // we want to return true if lock file does no longer exist, otherwise false.
}

score::os::IAccessControlList::UserIdentifier GetCreatorUidFromAcl(
    const ISharedMemoryResource::FileDescriptor fd,
    const ISharedMemoryResource::AccessControlListFactory& acl_factory) noexcept
{
    // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function
    // shall have automatic storage duration". Rationale: The object is a unique_ptr which is allocated in the heap.
    // coverity[autosar_cpp14_a18_5_8_violation]
    auto acl = acl_factory(fd);
    const auto result = acl->FindUserIdsWithPermission(score::os::Acl::Permission::kExecute);
    if (!result.has_value())
    {
        score::mw::log::LogFatal("shm") << "Finding creator_uid of shm-object failed: " << result.error();
        std::terminate();
    }

    const auto& users_with_exec_permission = result.value();
    // For named-shm in typedmemd, only creator of shm must have execute permission set in eACL.
    if (1U != users_with_exec_permission.size())
    {
        score::mw::log::LogFatal("shm") << "Invalid number of users with execution permission: Expected 1 user, found "
                                      << users_with_exec_permission.size();
        std::terminate();
    }

    return users_with_exec_permission.front();
}

ShmObjectStatInfo GetShmObjectStatInfo(const ISharedMemoryResource::FileDescriptor fd,
                                       const ISharedMemoryResource::AccessControlListFactory& acl_factory,
                                       bool is_named_shm)
{
    score::os::StatBuffer stat_buffer{};

    const auto result = score::os::Stat::instance().fstat(fd, stat_buffer);
    if (!result.has_value())
    {
        score::mw::log::LogFatal("shm") << "Getting owner_uid and size of shm-object file failed: " << result.error();
        std::terminate();
    }
    const auto owner_uid = stat_buffer.st_uid;
    bool is_shm_in_typed_memory = false;
    if (is_named_shm && (kTypedmemdUid == owner_uid))
    {
        is_shm_in_typed_memory = true;
        score::mw::log::LogInfo("shm") << "Named-shm is in TypedMemory. Finding creator-uid from from eACL entries.";
        stat_buffer.st_uid = GetCreatorUidFromAcl(fd, acl_factory);
    }
    // Suppress "AUTOSAR C++14 A0-1-1", The rule states: "A project shall not contain instances of non-volatile
    // variables being given values that are not subsequently used"
    // Rationale: There is no variable defined in the following line.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(stat_buffer.st_size >= 0, "Size of shm-object file is negative.");
    return {safe_math::Cast<uid_t>(stat_buffer.st_uid).value(),
            safe_math::Cast<std::size_t>(stat_buffer.st_size).value(),
            is_shm_in_typed_memory};
}

void* CalculateUsableStartAddress(void* const base_address, const std::size_t management_space) noexcept
{
    return AddOffsetToPointer(base_address, management_space);
}

}  // namespace

namespace detail
{

void* do_allocation_algorithm(const void* const alloc_start,
                              const void* const alloc_end,
                              const std::size_t bytes,
                              const std::size_t alignment) noexcept
{
    // Suppress "AUTOSAR C++14 A5-2-3" rule finding: A cast shall not remove any const or volatile
    // qualification from the type of a pointer or reference.
    // Rationale : std::align does not modify the underlying memory of aligned_address
    // (https://timsong-cpp.github.io/cppwp/n4659/ptr.align#lib:align) so the const_cast will not result in undefined
    // behaviour.
    // coverity[autosar_cpp14_a5_2_3_violation]
    void* aligned_address = const_cast<void*>(alloc_start);
    auto buffer_space = static_cast<std::size_t>(SubtractPointersBytes(alloc_end, alloc_start));
    return std::align(alignment, bytes, aligned_address, buffer_space);
}

}  // namespace detail

Stat::Mode SharedMemoryResource::calcStatModeForPermissions(const UserPermissions& permissions) noexcept
{
    if (std::holds_alternative<WorldWritable>(permissions))
    {
        score::mw::log::LogDebug("shm")
            << "Calculating Stat::Mode for SharedMemoryResource permissions: readWriteAccessForEveryBody";
        return readWriteAccessForEveryBody;
    }
    else
    {
        if (std::holds_alternative<WorldReadable>(permissions))
        {
            score::mw::log::LogDebug("shm")
                << "Calculating Stat::Mode for SharedMemoryResource permissions: readAccessForEveryBody";
            return readAccessForEveryBody;
        }
        else
        {
            score::mw::log::LogDebug("shm")
                << "Calculating Stat::Mode for SharedMemoryResource permissions: readWriteAccessForUser";
            return readWriteAccessForUser;
        }
    }
}

SharedMemoryResource::~SharedMemoryResource()
{
    MemoryResourceRegistry::getInstance().remove_resource(memory_identifier_);
    if (this->file_descriptor_ != -1)
    {
        this->deinitalizeInternalsInSharedMemory();
        score::cpp::ignore = ::score::os::Mman::instance().munmap(this->base_address_, virtual_address_space_to_reserve_);
        // The reason for banning is, because it's error-prone to use. One should use abstractions e.g. provided by
        // the C++ standard library. For Shared Memory handling there is no abstraction, which is why we created this
        // library.
        // NOLINTNEXTLINE(score-banned-function): See above.
        score::cpp::ignore = ::score::os::Unistd::instance().close(this->file_descriptor_);
    }
}

// Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
// namespace, or static function with internal linkage, or private member function shall be used.".
// False-positive, this private function is used via friend classes in tests and in SharedMemoryFactory.
// coverity[autosar_cpp14_a0_1_3_violation : FALSE]
void SharedMemoryResource::UnlinkFilesystemEntry() const noexcept
{
    // Unlinking the Shared Memory Resource only makes sense if it is a named Shared Memory Resource
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    // LCOV_EXCL_BR_START (Defensive programming: UnlinkFilesystemEntry is currently only called from
    // SharedMemoryFactory::Remove which removes a shared memory path. Therefore, shared_memory_resource_identifier_
    // will always contain a std::string (i.e. corresponding to a named SharedMemoryResource) in this function, so
    // there's no way for path to equal nullptr using the public interface.)
    if (path != nullptr)
    {
        // This requirement broken_link_c/issue/57467 directly excludes memory::shared (which is
        // part of mw::com) from the ban by listing it in the not relevant for field.
        // NOLINTNEXTLINE(score-banned-function): explanation on lines above.
        score::cpp::ignore = ::score::os::Mman::instance().shm_unlink(path->c_str());
    }
    // LCOV_EXCL_BR_STOP
}

// coverity[autosar_cpp14_a0_1_3_violation : FALSE] See rationale for autosar_cpp14_a0_1_3_violation above.
score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResource::Create(
    std::string input_path,
    const std::size_t user_space_to_reserve,
    InitializeCallback initialize_callback,
    const UserPermissions& permissions,
    AccessControlListFactory acl_factory,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    auto resource = CreateInstance(std::move(input_path), std::move(acl_factory), typed_memory_ptr);
    const auto result = resource->CreateImpl(user_space_to_reserve, std::move(initialize_callback), permissions);
    if (!result.has_value())
    {
        score::mw::log::LogError("shm") << "Unexpected error while creating Shared Memory Resource with errno"
                                      << result.error();

        return score::cpp::make_unexpected(result.error());
    }
    return resource;
}

// coverity[autosar_cpp14_a0_1_3_violation : FALSE] See rationale for autosar_cpp14_a0_1_3_violation above.
score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResource::CreateAnonymous(
    std::uint64_t shared_memory_resource_id,
    const std::size_t user_space_to_reserve,
    InitializeCallback initialize_callback,
    const UserPermissions& permissions,
    AccessControlListFactory acl_factory,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    auto resource = CreateInstance(shared_memory_resource_id, std::move(acl_factory), typed_memory_ptr);
    const auto result = resource->CreateImpl(user_space_to_reserve, std::move(initialize_callback), permissions);
    // LCOV_EXCL_START (Defensive programming: CreateAnonymous either returns a valid result or terminates.)
    // LCOV_EXCL_BR_START (See line coverage suppression explanation)
    if (!result.has_value())
    {
        score::mw::log::LogError("shm") << "Unexpected error while creating anonymous shared-memory resource with errno"
                                      << result.error();

        return score::cpp::make_unexpected(result.error());
    }
    // LCOV_EXCL_BR_STOP
    // LCOV_EXCL_STOP
    return resource;
}

// coverity[autosar_cpp14_a0_1_3_violation : FALSE] See rationale for autosar_cpp14_a0_1_3_violation above.
score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResource::CreateOrOpen(
    std::string input_path,
    const std::size_t user_space_to_reserve,
    InitializeCallback initialize_callback,
    const UserPermissions& permissions,
    AccessControlListFactory acl_factory,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    auto resource = CreateInstance(std::move(input_path), std::move(acl_factory), typed_memory_ptr);
    const auto result = resource->CreateOrOpenImpl(user_space_to_reserve, std::move(initialize_callback), permissions);
    if (!result.has_value())
    {
        score::mw::log::LogError("shm") << "Unexpected error while creating or opening shared-memory resource with errno"
                                      << result.error();

        return score::cpp::make_unexpected(result.error());
    }
    return resource;
}

// coverity[autosar_cpp14_a0_1_3_violation : FALSE] See rationale for autosar_cpp14_a0_1_3_violation above.
score::cpp::expected<std::shared_ptr<SharedMemoryResource>, score::os::Error> SharedMemoryResource::Open(
    std::string input_path,
    const bool is_read_write,
    AccessControlListFactory acl_factory,
    std::shared_ptr<score::memory::shared::TypedMemory> typed_memory_ptr) noexcept
{
    auto resource = CreateInstance(std::move(input_path), std::move(acl_factory), typed_memory_ptr);
    const auto result = resource->OpenImpl(is_read_write);
    if (!result.has_value())
    {
        score::mw::log::LogError("shm") << "Unexpected error while opening Shared Memory Resource with errno"
                                      << result.error();

        return score::cpp::make_unexpected(result.error());
    }
    return resource;
}

SharedMemoryResource::SharedMemoryResource(std::string input_path,
                                           AccessControlListFactory acl_factory,
                                           std::shared_ptr<TypedMemory> typed_memory_ptr) noexcept
    : ISharedMemoryResource{},
      std::enable_shared_from_this<SharedMemoryResource>{},
      file_descriptor_{-1},
      file_owner_uid_{static_cast<uid_t>(-1)},
      lock_file_path_{GetLockFilePath(input_path)},
      virtual_address_space_to_reserve_{},
      typed_memory_ptr_{typed_memory_ptr},
      opening_mode_{::score::os::Fcntl::Open::kReadOnly},
      map_mode_{::score::os::Mman::Protection::kRead},
      base_address_{nullptr},
      control_block_{nullptr},
      acl_factory_{std::move(acl_factory)},
      is_shm_in_typed_memory_{false},
      log_identification_{"file: " + input_path},
      memory_identifier_{score::cpp::hash_bytes(input_path.data(), input_path.size())},
      shared_memory_resource_identifier_{input_path},
      start_{nullptr}
{
    // We use memory_identifier_ == 0 as a sentinel value in OffsetPtr to indicate that the OffsetPtr doesn't belong to
    // a MemoryResource. Therefore, memory_identifier_ can never be 0U. With the current implementation of
    // score::cpp::hash_bytes, can not be 0 as long as input_path.size() is not 0.
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(memory_identifier_ != 0U, "");
}

SharedMemoryResource::SharedMemoryResource(std::uint64_t shared_memory_resource_id,
                                           AccessControlListFactory acl_factory,
                                           std::shared_ptr<TypedMemory> typed_memory_ptr) noexcept
    : ISharedMemoryResource{},
      std::enable_shared_from_this<SharedMemoryResource>{},
      file_descriptor_{-1},
      file_owner_uid_{static_cast<uid_t>(-1)},
      lock_file_path_{std::nullopt},
      virtual_address_space_to_reserve_{},
      typed_memory_ptr_{typed_memory_ptr},
      opening_mode_{::score::os::Fcntl::Open::kReadOnly},
      map_mode_{::score::os::Mman::Protection::kRead},
      base_address_{nullptr},
      control_block_{nullptr},
      acl_factory_{std::move(acl_factory)},
      is_shm_in_typed_memory_{false},
      log_identification_{"id: " + std::to_string(shared_memory_resource_id)},
      memory_identifier_{shared_memory_resource_id},
      shared_memory_resource_identifier_{shared_memory_resource_id},
      start_{nullptr}
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(memory_identifier_ != 0U, "");
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly."
// Rationale: score::Result value() can throw an exception if it's called without a value. Since we check has_value()
// before calling value(), an exception will never be called and therefore there will never be an implicit
// std::terminate call.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
score::cpp::expected_blank<Error> SharedMemoryResource::CreateImpl(const std::size_t user_space_to_reserve,
                                                            const InitializeCallback initialize_callback,
                                                            const UserPermissions& permissions) noexcept
{
    this->opening_mode_ = Fcntl::Open::kReadWrite;
    this->map_mode_ = ::score::os::Mman::Protection::kRead | ::score::os::Mman::Protection::kWrite;
    auto flags = Fcntl::Open::kReadWrite | Fcntl::Open::kCreate | Fcntl::Open::kExclusive;
    this->virtual_address_space_to_reserve_ = safe_math::Add(user_space_to_reserve, GetNeededManagementSpace()).value();

    std::optional<LockFile> lock_file{};
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    if (path != nullptr)
    {
        const auto create_lock_file_result = CreateLockFileForNamedSharedMemory(lock_file);
        if (!create_lock_file_result.has_value())
        {
            return score::cpp::make_unexpected(create_lock_file_result.error());
        }
    }

    const auto mode = calcStatModeForPermissions(permissions);
    // Try first to allocate the memory region in typed memory
    if (typed_memory_ptr_ != nullptr)
    {
        AllocateInTypedMemory(permissions, flags);
    }

    const auto open_result = OpenSharedMemory(flags, mode);
    if (!open_result.has_value())
    {
        return score::cpp::make_unexpected(open_result.error());
    }

    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(file_descriptor_ >= 0, "No valid file descriptor");

    const auto stat_values = GetShmObjectStatInfo(file_descriptor_, acl_factory_, (path != nullptr));
    file_owner_uid_ = stat_values.owner_uid;

    if (!is_shm_in_typed_memory_)
    {
        this->CompensateUmask(mode);
        this->ApplyPermissions(permissions);
        SealAnonymousOrReserveNamedSharedMemory();
    }
    this->mapMemoryIntoProcess();
    // Initialize what _we_ need
    this->initializeInternalsInSharedMemory();
    // Initialize what the _user_ needs
    initialize_callback(getSharedPtr());

    return {};
}

score::cpp::expected_blank<Error> SharedMemoryResource::CreateOrOpenImpl(const std::size_t user_space_to_reserve,
                                                                  InitializeCallback initialize_callback,
                                                                  const UserPermissions& permissions) noexcept
{
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(path != nullptr, "shm-object file path is not set.");
    this->opening_mode_ = Fcntl::Open::kReadWrite;
    this->map_mode_ = ::score::os::Mman::Protection::kRead | ::score::os::Mman::Protection::kWrite;
    constexpr bool open_read_write{true};
    const auto open_result = OpenImpl(open_read_write);
    if (!open_result.has_value())
    {
        if (open_result.error() == Error::Code::kNoSuchFileOrDirectory)
        {
            score::mw::log::LogDebug("shm") << "Could not open shared-memory resource with path" << *path << "with errno"
                                          << open_result.error() << "Attempting to create it now instead.";

            const auto creation_result = CreateImpl(user_space_to_reserve, std::move(initialize_callback), permissions);
            // If the shared memory segment could not be created because another process has created it or has acquired
            // the lock to create it, wait for the other process to create it.
            if (!creation_result.has_value())
            {
                // Create() should terminate for any other error
                std::stringstream s{};
                s << "Creating shared memory region failed with errno:" << creation_result.error();
                SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(((creation_result.error() == Error::Code::kDeviceOrResourceBusy) ||
                                        (creation_result.error() == Error::Code::kObjectExists)),
                                       s.str().c_str());

                score::mw::log::LogDebug("shm")
                    << "Could not create shared-memory region with errno:" << creation_result.error()
                    << "Another process is creating or has already created it. Attempting to open.";

                return OpenImpl(open_read_write);
            }
        }
        else
        {
            score::mw::log::LogFatal("shm")
                << __func__ << __LINE__ << "Unexpected error while opening Shared Memory Resource" << *path
                << "with errno" << open_result.error();
            std::terminate();
        }
    }
    return {};
}

score::cpp::expected_blank<Error> SharedMemoryResource::OpenImpl(const bool is_read_write) noexcept
{
    if (is_read_write)
    {
        this->opening_mode_ = Fcntl::Open::kReadWrite;
        this->map_mode_ = ::score::os::Mman::Protection::kRead | ::score::os::Mman::Protection::kWrite;
    }
    return this->waitForOtherProcessAndOpen();
}

// Warning is due to score::cpp::expected value() but the rationale is the same as for score::Result::value() above
// coverity[autosar_cpp14_a15_5_3_violation]
auto SharedMemoryResource::waitForOtherProcessAndOpen() noexcept -> score::cpp::expected_blank<Error>
{
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    const auto is_named_shm = (path != nullptr);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(is_named_shm, "shm-object file path is not set.");
    this->waitUntilInitializedByOtherProcess();

    // NOLINTNEXTLINE(score-banned-function) need to use shm_open for correct behavior
    const auto result = ::score::os::Mman::instance().shm_open(path->data(), this->opening_mode_, read_only);
    if (!result.has_value())
    {
        score::mw::log::LogError("shm") << __func__ << __LINE__ << "Unexpected error while opening Shared Memory Resource"
                                      << *path << "with errno" << result.error();
        return score::cpp::make_unexpected(result.error());
    }
    file_descriptor_ = result.value();
    const auto stat_values = GetShmObjectStatInfo(file_descriptor_, acl_factory_, is_named_shm);
    is_shm_in_typed_memory_ = stat_values.is_shm_in_typed_memory;
    file_owner_uid_ = stat_values.owner_uid;
    virtual_address_space_to_reserve_ = stat_values.size;
    this->loadInternalsFromSharedMemory();
    return {};
}

auto SharedMemoryResource::loadInternalsFromSharedMemory() noexcept -> void
{
    this->mapMemoryIntoProcess();
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding: "An object with integer type or pointer to void type shall not be
    // converted to an object with pointer type".
    // In initializeControlBlock(), a ControlBlock object is created at this->base_address_ and is never deallocated.
    // Since this->base_address_ is not modified after initializing the shared memory region,
    // we can safely assume that a ControlBlock object remains at this->base_address_.
    // coverity[autosar_cpp14_m5_2_8_violation]
    this->control_block_ = static_cast<ControlBlock*>(this->base_address_);
    this->start_ = CalculateUsableStartAddress(this->base_address_, GetNeededManagementSpace());
}

auto SharedMemoryResource::initializeInternalsInSharedMemory() noexcept -> void
{
    this->initializeControlBlock();
}

auto SharedMemoryResource::deinitalizeInternalsInSharedMemory() noexcept -> void
{
    // We do not de-initialize our memory part. This would cause problems
    // in the case that a process just restarted. Then the memory region would still be there,
    // the process would think he can continue, but would see uninitialized memory.
    // This would make more sense, if on shutdown of an process, we destroy the memory region.
    // But this is also not possible, since maybe the other process still access it.
}

// coverity[autosar_cpp14_a0_1_3_violation : FALSE] See rationale for autosar_cpp14_a0_1_3_violation above.
auto SharedMemoryResource::getOwnerUid() const noexcept -> uid_t
{
    return this->file_owner_uid_;
}

auto SharedMemoryResource::GetLockFilePath(const std::string& input_path) noexcept -> std::string
{
    return std::string{kTmpPathPrefix} + input_path + "_lock";
}

auto SharedMemoryResource::getMemoryResourceProxy() noexcept -> const MemoryResourceProxy*
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(this->control_block_ != nullptr,
                           "Control block containing MemoryResourceProxy has not yet been created.");

    return &this->control_block_->memoryResourceProxy;
}

auto SharedMemoryResource::do_allocate(const std::size_t bytes, const std::size_t alignment) -> void*
{
    std::lock_guard<score::os::InterprocessMutex> lock(this->control_block_->mutex);
    /**
     * For the proof of concept we agreed to use a monotonic allocation algorithm.
     * So there is no need for any fancy logic.
     */
    void* const allocation_start_address =
        AddOffsetToPointer(this->base_address_, this->control_block_->alreadyAllocatedBytes.load());
    void* const allocation_end_address = AddOffsetToPointer(this->base_address_, virtual_address_space_to_reserve_);
    void* const new_address_aligned =
        detail::do_allocation_algorithm(allocation_start_address, allocation_end_address, bytes, alignment);

    if (new_address_aligned == nullptr)
    {
        score::mw::log::LogFatal("shm") << "Cannot allocate shared memory block of size" << bytes << " at: ["
                                      << PointerToLogValue(new_address_aligned) << ":"
                                      << PointerToLogValue(AddOffsetToPointer(new_address_aligned, bytes))
                                      << "]. Does not fit within shared memory segment: ["
                                      << PointerToLogValue(this->base_address_) << ":"
                                      << PointerToLogValue(this->getEndAddress()) << "]";
        std::terminate();
    }
    const auto padding = SubtractPointersBytes(new_address_aligned, allocation_start_address);

    const auto total_allocated_bytes = safe_math::Add(bytes, padding).value();
    this->control_block_->alreadyAllocatedBytes += total_allocated_bytes;
    return new_address_aligned;
}

auto SharedMemoryResource::getBaseAddress() const noexcept -> void*
{
    // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
    // references to private or protected data owned by the class.".
    // The function provides controlled access to the internal 'base_address_' member. 'base_address_' is part of the
    // class's implementation details and is not meant for direct external access.
    //
    // By returning a pointer to 'base_address_', this function serves as an interface to access
    // this member, a common pattern in C++ to maintain encapsulation while exposing necessary internals.
    //
    // Although AUTOSAR C++14 rule A9-3-1 discourages returning addresses of non-static class members,
    // in this context, the design choice is deliberate and justified to provide controlled access,
    // hence the warning is suppressed.
    // Suppress "AUTOSAR C++14 M9-3-1" rule finding: "Const member functions shall not return non-const pointers or
    // references to class-data."
    // Rationale: In the long term, this function will return a pointer to const (will be done in Ticket-170815). However,
    // since we have many users of this function whose code would need to be updated, this isn't a priority for the
    // moment. While this function returns a non-const pointer to shared memory, our safety requirements and high level
    // design ensure that modifying this data (by a QM process for example) cannot lead to violations of safety goals
    // (e.g. through restricting write access of certain processes, bounds checking etc.). Therefore, we suppress this
    // warning for now.
    // coverity[autosar_cpp14_m9_3_1_violation]
    // coverity[autosar_cpp14_a9_3_1_violation]
    return this->base_address_;
}

auto SharedMemoryResource::getUsableBaseAddress() const noexcept -> void*
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_MESSAGE(
        this->start_ != nullptr,
        "Defensive programming: Start address is set either when creating or opening a memory region.");
    return this->start_;
}

auto SharedMemoryResource::GetUserAllocatedBytes() const noexcept -> std::size_t
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
        this->control_block_ != nullptr,
        "If the control block was not created during construction, there has been an unexpected error.");
    // Suppress "AUTOSAR C++14 A4-7-1" rule finding. This rule states: "An integer expression shall not lead to
    // loss.".
    // Rationale: alreadyAllocatedBytes is initialized with GetNeededManagementSpace() and is never reduced in size.
    // Therefore, subtracting GetNeededManagementSpace() could never result in a value less than 0.
    // coverity[autosar_cpp14_a4_7_1_violation]
    return this->control_block_->alreadyAllocatedBytes - this->GetNeededManagementSpace();
}

auto SharedMemoryResource::getEndAddress() const noexcept -> const void*
{
    return AddOffsetToPointer(this->base_address_, this->virtual_address_space_to_reserve_);
}

auto SharedMemoryResource::getPath() const noexcept -> const std::string*
{
    return std::get_if<std::string>(&shared_memory_resource_identifier_);
}

auto SharedMemoryResource::GetFileDescriptor() const noexcept -> FileDescriptor
{
    return file_descriptor_;
}

auto SharedMemoryResource::IsShmInTypedMemory() const noexcept -> bool
{
    return is_shm_in_typed_memory_;
}

auto SharedMemoryResource::do_deallocate(void*, std::size_t, std::size_t) -> void
{
    std::lock_guard<score::os::InterprocessMutex> lock(this->control_block_->mutex);
    /**
     * For the proof of concept we agreed to use a monotonic allocation algorithm.
     * Thus, no deallocation will be performed.
     */
}

auto SharedMemoryResource::do_is_equal(const memory_resource& other) const noexcept -> bool
{
    const auto* const otherCasted = dynamic_cast<const SharedMemoryResource*>(&other);
    if (otherCasted != nullptr)
    {
        return otherCasted->file_descriptor_ == this->file_descriptor_;
    }
    return false;
}

void SharedMemoryResource::CompensateUmask(const os::Stat::Mode target_rights) const noexcept
{
    if (target_rights == readWriteAccessForEveryBody)
    {
        auto result = score::os::Stat::instance().fchmod(file_descriptor_, target_rights);
        if (!result.has_value())
        {
            score::mw::log::LogWarn("shm") << "Unable to fchmod on shm-object" << log_identification_ << ": "
                                         << std::move(result).error();
        }
    }
}

auto SharedMemoryResource::ApplyPermissions(const UserPermissions& permissions) noexcept -> void
{
    if (std::holds_alternative<UserPermissionsMap>(permissions))
    {
        const auto& permission_map_ptr = std::get_if<UserPermissionsMap>(&permissions);
        SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(permission_map_ptr != nullptr, "Could not get user permissions map");

        // Suppress "AUTOSAR C++14 A18-5-8" rule finding. This rule states: "Objects that do not outlive a function
        // shall
        // have automatic storage duration".
        // The acl_factory_ is an score::cpp::callback which returns a std::unique_ptr. We return a unique_ptr since we want to
        // be able to use dynamic dispatch to mock the IAccessControlList but also want the caller of the callback to be
        // the sole owner of the IAccessControlList.
        // coverity[autosar_cpp14_a18_5_8_violation]
        auto acl = acl_factory_(file_descriptor_);
        for (const auto& permission : *permission_map_ptr)
        {
            for (const auto& user : permission.second)
            {
                auto result = acl->AllowUser(user, permission.first);
                if (!result.has_value())
                {
                    score::mw::log::LogError("shm")
                        << "Unable to set ACLs for user " << user << ": " << std::move(result).error();
                }
            }
        }
    }
}

auto SharedMemoryResource::reserveSharedMemory() const noexcept -> void
{
    const auto truncation_result = ::score::os::Unistd::instance().ftruncate(
        this->file_descriptor_, static_cast<off_t>(virtual_address_space_to_reserve_));
    if (!truncation_result.has_value())
    {
        score::mw::log::LogFatal("shm") << __func__ << __LINE__ << "Could not ftruncate file to size"
                                      << virtual_address_space_to_reserve_ << "for" << log_identification_
                                      << "with error" << truncation_result.error();
        std::terminate();
    }
}

auto SharedMemoryResource::mapMemoryIntoProcess() noexcept -> void
{
    // get all the memory _we_ need
    const auto result = ::score::os::Mman::instance().mmap(nullptr,
                                                         virtual_address_space_to_reserve_,
                                                         this->map_mode_,
                                                         ::score::os::Mman::Map::kShared,
                                                         this->file_descriptor_,
                                                         0);

    if (!result.has_value())
    {
        score::mw::log::LogFatal("shm") << __func__ << __LINE__
                                      << "Unexpected error while mapping memory into process for" << log_identification_
                                      << "with errno" << result.error() << ". Terminating.";
        std::terminate();
    }

    this->base_address_ = result.value();
    const bool inserted = MemoryResourceRegistry::getInstance().insert_resource({memory_identifier_, this});
    if (!inserted)
    {
        ::score::mw::log::LogFatal("shm")
            << __func__ << __LINE__ << "Inserting SharedMemoryResource for" << log_identification_
            << "into MemoryResourceRegistry failed. Either another SharedMemoryResource used same "
               "path / id or a hash-collision on path happened.";
        std::terminate();
    }
}

auto SharedMemoryResource::initializeControlBlock() noexcept -> void
{
    // base_address_ is the address we got back from mmap() call and it is therefore guaranteed to be page aligned!
    // Proper usage of the operator new according to Autosar rule A18-5-10.
    // NOLINTNEXTLINE(score-no-dynamic-raw-memory): Placement new is used.
    this->control_block_ = new (this->base_address_) ControlBlock(memory_identifier_);
    // we want the memory region, where later further allocations start from, to be "worst case aligned".
    // The main reason: Reproducibility of memory needs for a deterministic set of allocations.
    constexpr auto aligned_control_block_size = GetNeededManagementSpace();
    this->start_ = CalculateUsableStartAddress(this->base_address_, aligned_control_block_size);
    this->control_block_->alreadyAllocatedBytes = aligned_control_block_size;
}

// Warning is due to std::optional::value but the rationale is the same as for std::optional:value() above
// coverity[autosar_cpp14_a15_5_3_violation]
auto SharedMemoryResource::waitUntilInitializedByOtherProcess() const noexcept -> void
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lock_file_path_.has_value(), "Lock file path is not set.");
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(path != nullptr, "shm-object file path is not set.");
    if (!waitForFreeLockFile(lock_file_path_.value()))
    {
        // Lock file is still there after timeout, we cannot remove it and reinitialize,
        // so there is nothing for us to do besides dying.
        ::score::mw::log::LogFatal("shm")
            << __func__ << __LINE__ << "Shared Memory Resource: " << *path
            << "Lock file still present after timeout. Cannot open shared memory. Terminating";
        std::terminate();
    }
}

// Warning is due to std::optional::value but the rationale is the same as for std::optional:value() above
// coverity[autosar_cpp14_a15_5_3_violation]
score::cpp::expected_blank<score::os::Error> SharedMemoryResource::CreateLockFileForNamedSharedMemory(
    std::optional<LockFile>& lock_file) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lock_file_path_.has_value(), "Lock file path is not set.");
    lock_file = LockFile::Create(this->lock_file_path_.value());
    if (!lock_file.has_value())
    {
        score::mw::log::LogWarn("shm") << __func__ << __LINE__
                                     << "Unexpected error while creating Shared Memory Resource with"
                                     << log_identification_
                                     << ". The lock file is already locked indicating that the shared memory region is "
                                        "already being created.";
        return score::cpp::make_unexpected(Error::createFromErrno(EBUSY));
    }
    return {};
}

void SharedMemoryResource::AllocateInTypedMemory(const UserPermissions& permissions, Fcntl::Open& flags) noexcept
{
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    if (path != nullptr)
    {
        const auto allocate_named_typed_memory_result =
            typed_memory_ptr_->AllocateNamedTypedMemory(virtual_address_space_to_reserve_, path->c_str(), permissions);
        if (allocate_named_typed_memory_result.has_value())
        {
            score::mw::log::LogDebug("shm") << __func__ << "Shm is in TypedMemory. Set file open flags";
            is_shm_in_typed_memory_ = true;
            flags = Fcntl::Open::kReadWrite | Fcntl::Open::kExclusive;
        }
        else
        {
            score::mw::log::LogWarn("shm")
                << __func__ << __LINE__
                << "Unexpected error while trying to allocate shared-memory in typed memory using "
                << log_identification_ << "Reason: " << allocate_named_typed_memory_result.error();
        }
    }
    const auto* const id = std::get_if<std::uint64_t>(&shared_memory_resource_identifier_);
    if (id != nullptr)
    {
        const auto allocate_anonymous_typed_memory_result =
            typed_memory_ptr_->AllocateAndOpenAnonymousTypedMemory(virtual_address_space_to_reserve_);
        if (allocate_anonymous_typed_memory_result.has_value())
        {
            is_shm_in_typed_memory_ = true;
            file_descriptor_ = allocate_anonymous_typed_memory_result.value();
            score::mw::log::LogInfo("shm") << __func__ << __LINE__
                                         << "Successfully allocated anonymous shared-memory in typed memory";
        }
        else
        {
            score::mw::log::LogWarn("shm")
                << __func__ << __LINE__
                << "Unexpected error while trying to allocate shared-memory in typed memory using "
                << log_identification_ << "Reason: " << allocate_anonymous_typed_memory_result.error();
        }
    }
}

score::cpp::expected_blank<score::os::Error> SharedMemoryResource::OpenSharedMemory(const Fcntl::Open& flags,
                                                                           Stat::Mode mode) noexcept
{
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    if (path != nullptr)
    {
        // NOLINTNEXTLINE(score-banned-function) need to use shm_open for correct behavior
        const auto result = ::score::os::Mman::instance().shm_open(path->data(), flags, mode);
        if (!result.has_value())
        {
            // If we couldn't create the memory region because it's already open, we return an error code.
            // Otherwise, we terminate.
            if (result.error() == Error::Code::kObjectExists)
            {
                score::mw::log::LogError("shm") << "Error while opening shared-memory Resource: ObjectExists";

                return score::cpp::make_unexpected(result.error());
            }
            else
            {
                score::mw::log::LogFatal("shm") << "Unexpected error while opening shared-memory Resource using"
                                              << log_identification_ << "with errno" << result.error();
                std::terminate();
            }
        }

        file_descriptor_ = result.value();
    }
    const auto* const id = std::get_if<std::uint64_t>(&shared_memory_resource_identifier_);
    if (id != nullptr)
    {
        if (!is_shm_in_typed_memory_)
        {
            const auto shm_open_result =
                score::memory::shared::SealedShm::instance().OpenAnonymous(::score::os::ModeToInteger(mode));
            if (!shm_open_result.has_value())
            {
                score::mw::log::LogFatal("shm") << "Unexpected error while opening anonymous shared-memory Resource"
                                              << "with errno" << shm_open_result.error();
                std::terminate();
            }
            else
            {
                file_descriptor_ = shm_open_result.value();
                score::mw::log::LogDebug("shm")
                    << "Successfully opened anonymous shared-memory Resource with shm file_descriptor:"
                    << file_descriptor_;
            }
        }
        // The else branch is omitted here on purpose. See doxygen for details.
        score::mw::log::LogInfo("shm") << __func__ << __LINE__ << "Set shared memory resource id:" << *id;
    }
    return {};
}

void SharedMemoryResource::SealAnonymousOrReserveNamedSharedMemory() noexcept
{
    const auto* const path = std::get_if<std::string>(&shared_memory_resource_identifier_);
    if (path != nullptr)
    {
        this->reserveSharedMemory();
    }
    const auto* const id = std::get_if<std::uint64_t>(&shared_memory_resource_identifier_);
    if (id != nullptr)
    {
        score::mw::log::LogInfo("shm") << __func__ << __LINE__ << "Sealing anonymous shared-memory resource";
        const auto seal_shm_result =
            score::memory::shared::SealedShm::instance().Seal(file_descriptor_, virtual_address_space_to_reserve_);
        if (!seal_shm_result.has_value())
        {
            score::mw::log::LogError("shm")
                << __func__ << __LINE__ << "Unexpected error while sealing anonymous Shared Memory Resource"
                << "with errno" << seal_shm_result.error();
            this->reserveSharedMemory();
        }
    }
}

}  // namespace score::memory::shared
