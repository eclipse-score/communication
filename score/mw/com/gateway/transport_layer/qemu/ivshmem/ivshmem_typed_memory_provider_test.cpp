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
#include "score/mw/com/gateway/transport_layer/qemu/ivshmem/ivshmem_typed_memory_provider.h"

#include "score/memory/shared/user_permission.h"
#include "score/os/mocklib/mman_mock.h"

#if defined(__QNXNTO__)
#include "score/os/mocklib/qnx/mock_mman.h"
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <string>

namespace score::mw::com::gateway::qemu::ivshmem
{

namespace
{

/// Test subclass that exposes protected members for white-box testing on non-QNX.
class TestableIvshmemTypedMemoryProvider : public IvshmemTypedMemoryProvider
{
  public:
    TestableIvshmemTypedMemoryProvider(std::uint64_t paddr, std::uint64_t size) noexcept
        : IvshmemTypedMemoryProvider{paddr, size}
    {
    }

#if defined(__QNXNTO__)
    /// Injection constructor — allows passing a MmanQnx mock for QNX tests.
    TestableIvshmemTypedMemoryProvider(std::uint64_t paddr,
                                       std::uint64_t size,
                                       std::unique_ptr<score::os::qnx::MmanQnx> mman_qnx) noexcept
        : IvshmemTypedMemoryProvider{paddr, size, std::move(mman_qnx)}
    {
    }
#endif

    /// Bring BindShmToBar into public scope so tests can invoke it directly.
    using IvshmemTypedMemoryProvider::BindShmToBar;

    /// Populate the local allocation cache, enabling tests for GetAllocationOffset's found-branch.
    void InsertAllocation(const std::string& name, std::uint64_t offset) noexcept
    {
        SetAllocationInCache(name, offset);
    }
};

class IvshmemTypedMemoryProviderTest : public ::testing::Test
{
  protected:
    static constexpr std::uint64_t kBarPaddr = 0x100000U;
    static constexpr std::uint64_t kBarSize = 1024U * 1024U;  // 1 MiB

    IvshmemTypedMemoryProvider provider_{kBarPaddr, kBarSize};
};

TEST_F(IvshmemTypedMemoryProviderTest, ConstructorWithNormalSizeCalculatesUsableSize)
{
    // When constructing with normal size
    IvshmemTypedMemoryProvider p{kBarPaddr, kBarSize};

    // Then construction succeeds without issues
    (void)p;
}

TEST_F(IvshmemTypedMemoryProviderTest, ConstructorWithSizeSmallerThanDirectorySetsUsableSizeToZero)
{
    // When BAR size is smaller than directory size
    IvshmemTypedMemoryProvider p{kBarPaddr, 100U};

    // Then construction succeeds with usable_size_ = 0
    (void)p;
}

TEST_F(IvshmemTypedMemoryProviderTest, ConstructorWithSizeEqualToDirectorySetsUsableSizeToZero)
{
    // When constructing with size equal to directory size
    IvshmemTypedMemoryProvider p{kBarPaddr, IvshmemTypedMemoryProvider::kDirectorySize};

    // Then construction succeeds with usable_size_ = 0
    (void)p;
}

TEST_F(IvshmemTypedMemoryProviderTest, ConstructorWithZeroSizeSetsUsableSizeToZero)
{
    // When constructing with zero size
    IvshmemTypedMemoryProvider p{kBarPaddr, 0U};

    // Then construction succeeds with usable_size_ = 0
    (void)p;
}

TEST(IvshmemTypedMemoryProviderHashNameTest, EmptyStringReturnsInitialSeed)
{
    // When hashing an empty string
    // Then FNV-1a returns the initial seed value
    EXPECT_EQ(IvshmemTypedMemoryProvider::HashName(""), 2166136261U);
}

TEST(IvshmemTypedMemoryProviderHashNameTest, SingleCharacterProducesExpectedHash)
{
    // When hashing a single character
    const std::uint32_t expected = (2166136261U ^ 65U) * 16777619U;

    // Then FNV-1a produces expected hash
    EXPECT_EQ(IvshmemTypedMemoryProvider::HashName("A"), expected);
}

TEST(IvshmemTypedMemoryProviderHashNameTest, SameInputAlwaysProducesSameHash)
{
    // When hashing the same input twice
    // Then both calls produce identical hashes
    EXPECT_EQ(IvshmemTypedMemoryProvider::HashName("/my_shm"), IvshmemTypedMemoryProvider::HashName("/my_shm"));
}

TEST(IvshmemTypedMemoryProviderHashNameTest, DifferentInputsProduceDifferentHashes)
{
    // When hashing different inputs
    // Then hashes are different
    EXPECT_NE(IvshmemTypedMemoryProvider::HashName("/shm_a"), IvshmemTypedMemoryProvider::HashName("/shm_b"));
}

// On QNX the function actually executes and may succeed; this test only checks the Linux #else stub.
#if !defined(__QNXNTO__)

TEST_F(IvshmemTypedMemoryProviderTest, AllocateNamedTypedMemoryReturnsEnosysOnNonQnx)
{
    // When calling AllocateNamedTypedMemory on non-QNX
    const auto result =
        provider_.AllocateNamedTypedMemory(4096U, "/test_shm", score::memory::shared::permission::WorldWritable{});

    // Then the stub returns error
    EXPECT_FALSE(result.has_value());
}

TEST_F(IvshmemTypedMemoryProviderTest, AllocateNamedTypedMemoryAtOffsetReturnsEnosysOnNonQnx)
{
    // When calling AllocateNamedTypedMemoryAtOffset on non-QNX
    const auto result = provider_.AllocateNamedTypedMemoryAtOffset(
        4096U, "/test_shm", 0U, score::memory::shared::permission::WorldWritable{});

    // Then the stub returns error
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemTypedMemoryProviderBindShmToBarTest, ReturnsEnosysOnNonQnx)
{
    // Given a testable provider
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U};

    // When calling BindShmToBar on non-QNX
    const auto result = p.BindShmToBar(4096U, "/test_shm", 0U);

    // Then the stub returns error
    ASSERT_FALSE(result.has_value());
}

#endif  // !defined(__QNXNTO__)

// Compiled only on QNX where mman_qnx_ is active and MmanQnxMock is available.
#if defined(__QNXNTO__)

TEST(IvshmemTypedMemoryProviderBindShmToBarTest, ReturnsErrorWhenShmOpenFails)
{
    // Given mock that fails on shm_open
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting shm_open to be called and return error
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM))));

    // When calling BindShmToBar
    const auto result = p.BindShmToBar(4096U, "/test_shm", 0U);

    // Then the operation fails
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemTypedMemoryProviderBindShmToBarTest, ReturnsErrorWhenShmCtlFails)
{
    // Given mock where shm_open succeeds but shm_ctl fails
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting shm_open to succeed and shm_ctl to fail
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(EINVAL))));

    // When calling BindShmToBar
    const auto result = p.BindShmToBar(4096U, "/test_shm", 0U);

    // Then the operation fails
    EXPECT_FALSE(result.has_value());
}

#endif  // defined(__QNXNTO__)

TEST_F(IvshmemTypedMemoryProviderTest, AllocateAndOpenAnonymousTypedMemoryReturnsEnosys)
{
    // When calling AllocateAndOpenAnonymousTypedMemory
    const auto result = provider_.AllocateAndOpenAnonymousTypedMemory(4096U);

    // Then the stub returns error
    EXPECT_FALSE(result.has_value());
}

TEST_F(IvshmemTypedMemoryProviderTest, UnlinkSucceedsWhenShmUnlinkSucceeds)
{
    // Given mock where shm_unlink succeeds
    score::os::MockGuard<score::os::MmanMock> mman_mock{};
    EXPECT_CALL(*mman_mock, shm_unlink(::testing::StrEq("/test_shm"))).WillOnce(::testing::Return(score::cpp::blank{}));

    // When calling Unlink
    const auto result = provider_.Unlink("/test_shm");

    // Then the operation succeeds
    EXPECT_TRUE(result.has_value());
}

TEST_F(IvshmemTypedMemoryProviderTest, UnlinkReturnsErrorWhenShmUnlinkFails)
{
    // Given mock where shm_unlink fails
    score::os::MockGuard<score::os::MmanMock> mman_mock{};
    EXPECT_CALL(*mman_mock, shm_unlink(::testing::StrEq("/test_shm")))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOENT))));

    // When calling Unlink
    const auto result = provider_.Unlink("/test_shm");

    // Then the operation fails
    EXPECT_FALSE(result.has_value());
}

TEST_F(IvshmemTypedMemoryProviderTest, GetCreatorUidReturnsCurrentUid)
{
    // When calling GetCreatorUid
    const auto result = provider_.GetCreatorUid("/test_shm");

    // Then the current UID is returned
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), ::getuid());
}

TEST_F(IvshmemTypedMemoryProviderTest, GetAllocationOffsetReturnsNulloptForUnknownName)
{
    // When calling GetAllocationOffset with unknown name
    const auto result = provider_.GetAllocationOffset("/unknown");

    // Then nullopt is returned
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemTypedMemoryProviderGetAllocationOffsetTest, ReturnsOffsetWhenAllocationExists)
{
    // Given a provider with an allocation in the cache
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U};
    p.InsertAllocation("/my_shm", 8192U);

    // When calling GetAllocationOffset
    const auto result = p.GetAllocationOffset("/my_shm");

    // Then the cached offset is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 8192U);
}

TEST(IvshmemTypedMemoryProviderGetAllocationOffsetTest, ReturnsNulloptForUninsertedName)
{
    // Given a provider with a different allocation cached
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U};
    p.InsertAllocation("/other_shm", 4096U);

    // When calling GetAllocationOffset with a name not in cache
    // Then nullopt is returned
    EXPECT_FALSE(p.GetAllocationOffset("/my_shm").has_value());
}

// On QNX the function actually executes MapDirectory() against physical memory; only test on Linux.
#if !defined(__QNXNTO__)

TEST_F(IvshmemTypedMemoryProviderTest, LookupOffsetInDirectoryReturnsNulloptOnNonQnx)
{
    // When calling LookupOffsetInDirectory on non-QNX
    const auto result = provider_.LookupOffsetInDirectory("/test_shm");

    // Then the stub returns nullopt
    EXPECT_FALSE(result.has_value());
}

#endif  // !defined(__QNXNTO__)

#if defined(__QNXNTO__)

// Helper: build a zero-initialised directory buffer and optionally fill N entries.
// Each synthetic entry uses a unique non-colliding hash and sequential 4 KiB offsets.
static void FillDirectoryEntries(std::uint8_t* buf, std::uint32_t count) noexcept
{
    *reinterpret_cast<std::uint32_t*>(buf) = 0U;
    *reinterpret_cast<std::uint32_t*>(buf + sizeof(std::uint32_t)) = count;
    auto* entries = reinterpret_cast<IvshmemTypedMemoryProvider::DirectoryEntry*>(
        buf + IvshmemTypedMemoryProvider::kDirectoryHeaderSize);
    for (std::uint32_t i = 0U; i < count; ++i)
    {
        entries[i].name_hash = 0xDEAD0000U + i;  // chosen to not collide with any test name
        entries[i].bar_offset = static_cast<std::uint64_t>(i) * 4096U;
        entries[i].alloc_size = 4096U;
    }
}

TEST(IvshmemQnxMapDirectoryTest, MmapFailureReturnsNulloptFromLookup)
{
    // Given mock where mmap fails
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting mmap to fail
    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM))));

    // When calling LookupOffsetInDirectory
    // Then nullopt is returned
    EXPECT_FALSE(p.LookupOffsetInDirectory("/test_shm").has_value());
}

TEST(IvshmemQnxMapDirectoryTest, DirectoryMapIsCachedAfterFirstCall)
{
    // Given a provider with a mock that maps directory to buffer
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting mmap to be called exactly once
    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));

    // When calling LookupOffsetInDirectory twice
    EXPECT_FALSE(p.LookupOffsetInDirectory("/first").has_value());
    EXPECT_FALSE(p.LookupOffsetInDirectory("/second").has_value());

    // Then mmap is called only once due to caching
}

TEST(IvshmemQnxLookupOffsetTest, ReturnsOffsetWhenEntryFoundInDirectory)
{
    // Given a directory buffer with matching entry
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    const std::uint32_t target_hash = IvshmemTypedMemoryProvider::HashName("/shm_target");
    *reinterpret_cast<std::uint32_t*>(buf.data() + sizeof(std::uint32_t)) = 1U;
    auto* entry = reinterpret_cast<IvshmemTypedMemoryProvider::DirectoryEntry*>(
        buf.data() + IvshmemTypedMemoryProvider::kDirectoryHeaderSize);
    entry->name_hash = target_hash;
    entry->bar_offset = 8192U;
    entry->alloc_size = 4096U;
    std::strncpy(entry->name, "/shm_target", IvshmemTypedMemoryProvider::kMaxNameLength - 1U);

    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));

    // When looking up an entry that exists
    const auto result = p.LookupOffsetInDirectory("/shm_target");

    // Then the offset is returned
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 8192U);
}

TEST(IvshmemQnxLookupOffsetTest, ReturnsNulloptWhenEntryNotFoundInDirectory)
{
    // Given a directory buffer with a different entry
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    *reinterpret_cast<std::uint32_t*>(buf.data() + sizeof(std::uint32_t)) = 1U;
    auto* entry = reinterpret_cast<IvshmemTypedMemoryProvider::DirectoryEntry*>(
        buf.data() + IvshmemTypedMemoryProvider::kDirectoryHeaderSize);
    entry->name_hash = IvshmemTypedMemoryProvider::HashName("/other_shm");
    entry->bar_offset = 4096U;
    entry->alloc_size = 4096U;

    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));

    // When looking up an entry that doesn't exist
    // Then nullopt is returned
    EXPECT_FALSE(p.LookupOffsetInDirectory("/test_shm").has_value());
}

TEST(IvshmemTypedMemoryProviderBindShmToBarTest, SucceedsWhenShmOpenAndShmCtlSucceed)
{
    // Given mock where shm_open and shm_ctl succeed
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting shm_open and shm_ctl calls to succeed
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When calling BindShmToBar
    const auto result = p.BindShmToBar(4096U, "/test_shm", 0U);

    // Then the operation succeeds
    EXPECT_TRUE(result.has_value());
}

TEST(IvshmemQnxAllocateNamedTest, LocalCacheHitBindsShm)
{
    // Given a provider with allocation already in local cache
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};
    p.InsertAllocation("/test_shm", 4096U);

    // Expecting shm_open and shm_ctl to be called
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When allocating the same name
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/test_shm", score::memory::shared::permission::WorldWritable{});

    // Then the allocation succeeds using cached offset
    EXPECT_TRUE(result.has_value());
}

TEST(IvshmemQnxAllocateNamedTest, DirectoryHitBindsShm)
{
    // Given a directory with matching entry
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    const std::uint32_t target_hash = IvshmemTypedMemoryProvider::HashName("/test_shm");
    // Write count=1 at the correct header offset (lock_word is at offset 0, count at offset 4)
    *reinterpret_cast<std::uint32_t*>(buf.data() + sizeof(std::uint32_t)) = 1U;
    auto* entry = reinterpret_cast<IvshmemTypedMemoryProvider::DirectoryEntry*>(
        buf.data() + IvshmemTypedMemoryProvider::kDirectoryHeaderSize);
    entry->name_hash = target_hash;
    entry->bar_offset = 4096U;
    entry->alloc_size = 4096U;
    std::strncpy(entry->name, "/test_shm", IvshmemTypedMemoryProvider::kMaxNameLength - 1U);

    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting mmap, shm_open, and shm_ctl to be called
    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When allocating a name found in directory
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/test_shm", score::memory::shared::permission::WorldWritable{});

    // Then allocation succeeds and entry is cached
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(p.GetAllocationOffset("/test_shm").has_value());
}

TEST(IvshmemQnxAllocateNamedTest, ReturnsEnomemWhenBarExhausted)
{
    // Given provider with exhausted usable space (100 bytes < 4 KiB request)
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{
        0x100000U, IvshmemTypedMemoryProvider::kDirectorySize + 100U, std::move(mman_mock)};

    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));

    // When attempting to allocate
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/test_shm", score::memory::shared::permission::WorldWritable{});

    // Then allocation fails due to insufficient space
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemQnxAllocateNamedTest, NewAllocationSucceedsAndCaches)
{
    // Given provider with empty directory
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting mmap once and both shm_open and shm_ctl calls
    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When allocating a new named typed memory
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/test_shm", score::memory::shared::permission::WorldWritable{});

    // Then allocation succeeds and is cached for future lookups
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(p.GetAllocationOffset("/test_shm").has_value());
}

// This covers WriteDirectoryEntry's count-limit guard; the allocation still succeeds via BindShmToBar.

TEST(IvshmemQnxAllocateNamedTest, FullDirectoryStillSucceedsViaBindShmToBar)
{
    // Given a directory with maximum entries filled
    constexpr std::uint64_t kLargeBarSize = 2U * 1024U * 1024U;
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    FillDirectoryEntries(buf.data(), IvshmemTypedMemoryProvider::kMaxDirectoryEntries);

    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, kLargeBarSize, std::move(mman_mock)};

    // Expecting mmap, shm_open, and shm_ctl calls
    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When allocating a new name when directory is full
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/new_shm", score::memory::shared::permission::WorldWritable{});

    // Then allocation still succeeds via BindShmToBar
    EXPECT_TRUE(result.has_value());
}

TEST(IvshmemQnxAllocateAtOffsetTest, ReturnsEnomemWhenBarExhausted)
{
    // Given provider with exhausted usable space
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    TestableIvshmemTypedMemoryProvider p{
        0x100000U, IvshmemTypedMemoryProvider::kDirectorySize + 100U, std::move(mman_mock)};

    // When attempting to allocate at specific offset
    const auto result =
        p.AllocateNamedTypedMemoryAtOffset(4096U, "/test_shm", 0U, score::memory::shared::permission::WorldWritable{});

    // Then allocation fails
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemQnxAllocateAtOffsetTest, ReturnsEexistWhenOffsetMismatch)
{
    // Given a provider with a cached allocation at different offset
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};
    p.InsertAllocation("/test_shm", 4096U);

    // When attempting to allocate the same name at different offset
    const auto result = p.AllocateNamedTypedMemoryAtOffset(
        4096U, "/test_shm", 8192U, score::memory::shared::permission::WorldWritable{});

    // Then allocation fails due to offset mismatch
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemQnxAllocateAtOffsetTest, MatchingOffsetInCacheBindsShm)
{
    // Given a provider with cached allocation at specified offset
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};
    p.InsertAllocation("/test_shm", 4096U);

    // Expecting shm_open and shm_ctl to be called
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When allocating at the matching cached offset
    const auto result = p.AllocateNamedTypedMemoryAtOffset(
        4096U, "/test_shm", 4096U, score::memory::shared::permission::WorldWritable{});

    // Then allocation succeeds
    EXPECT_TRUE(result.has_value());
}

TEST(IvshmemQnxAllocateAtOffsetTest, NewEntrySucceedsAndCaches)
{
    // Given a provider with no prior allocation
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting shm_open and shm_ctl to be called
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When allocating new entry at specific offset
    const auto result = p.AllocateNamedTypedMemoryAtOffset(
        4096U, "/test_shm", 4096U, score::memory::shared::permission::WorldWritable{});

    // Then allocation succeeds and offset is cached
    EXPECT_TRUE(result.has_value());
    ASSERT_TRUE(p.GetAllocationOffset("/test_shm").has_value());
    EXPECT_EQ(p.GetAllocationOffset("/test_shm").value(), 4096U);
}

#endif  // defined(__QNXNTO__)

#if defined(__QNXNTO__)

// Flow: LookupOffset→mmap fails→nullopt; FindNextFreeOffset→mmap fails→return 0U;
//       WriteDirectoryEntry→mmap fails→early return; BindShmToBar succeeds normally.

TEST(IvshmemQnxNullDirPathsTest, MmapAlwaysFailsCoversNullDirBranches)
{
    // Given provider with mmap that always fails
    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting mmap to fail; shm_open/shm_ctl must NOT be called since we return ENOMEM early.
    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM))));

    // When allocating when mmap fails
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/test_shm", score::memory::shared::permission::WorldWritable{});

    // Then allocation fails: without a mapped directory there is no safe place to record the
    // allocation, so the provider returns ENOMEM rather than silently placing everything at offset 0.
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemQnxWriteDirEntryTest, PrefilledNonMatchingEntriesLoopIteratesBeforeAppend)
{
    // Given directory buffer with non-matching entries
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    FillDirectoryEntries(buf.data(), 2U);

    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    // Expecting mmap, shm_open, and shm_ctl calls
    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));
    EXPECT_CALL(*raw, shm_open(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(std::int32_t{-2}));
    EXPECT_CALL(*raw, shm_ctl(std::int32_t{-2}, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(std::int32_t{0}));

    // When allocating new entry with existing entries in directory
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/new_shm", score::memory::shared::permission::WorldWritable{});

    // Then allocation succeeds and new entry is appended and cached
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(p.GetAllocationOffset("/new_shm").has_value());
}

TEST(IvshmemQnxLookupOffsetTest, OversizedCountStopsAtMaxEntriesGuard)
{
    // Given directory buffer with oversized count exceeding limit
    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    FillDirectoryEntries(buf.data(), IvshmemTypedMemoryProvider::kMaxDirectoryEntries);
    *reinterpret_cast<std::uint32_t*>(buf.data() + sizeof(std::uint32_t)) =
        IvshmemTypedMemoryProvider::kMaxDirectoryEntries + 5U;

    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, 1024U * 1024U, std::move(mman_mock)};

    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));

    // When looking up entry with oversized count
    const auto result = p.LookupOffsetInDirectory("/safe_shm");

    // Then scan stops at max entries guard instead of count field
    EXPECT_FALSE(result.has_value());
}

TEST(IvshmemQnxLookupOffsetTest, OversizedCountAllocateCoversAllocateFindNextFreeOffset)
{
    // Given a tight BAR and a directory already filled to max entries.
    constexpr std::uint64_t kEntrySize = 4096U;
    constexpr std::uint64_t kTightBarSize =
        static_cast<std::uint64_t>(IvshmemTypedMemoryProvider::kMaxDirectoryEntries) * kEntrySize +
        IvshmemTypedMemoryProvider::kDirectorySize;

    std::array<std::uint8_t, IvshmemTypedMemoryProvider::kDirectorySize> buf{};
    FillDirectoryEntries(buf.data(), IvshmemTypedMemoryProvider::kMaxDirectoryEntries);
    // Write count at the correct header offset (count is at offset 4, not offset 0).
    // offset 0 = lock_word, must stay 0 so the spinlock can be acquired.
    *reinterpret_cast<std::uint32_t*>(buf.data() + sizeof(std::uint32_t)) =
        IvshmemTypedMemoryProvider::kMaxDirectoryEntries + 5U;

    auto mman_mock = std::make_unique<::testing::StrictMock<score::os::qnx::MmanQnxMock>>();
    auto* const raw = mman_mock.get();
    TestableIvshmemTypedMemoryProvider p{0x100000U, kTightBarSize, std::move(mman_mock)};

    EXPECT_CALL(*raw,
                mmap(nullptr,
                     IvshmemTypedMemoryProvider::kDirectorySize,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_,
                     ::testing::_))
        .WillOnce(::testing::Return(static_cast<void*>(buf.data())));

    // When allocating with oversized count exhausts BAR
    const auto result =
        p.AllocateNamedTypedMemory(4096U, "/safe_shm", score::memory::shared::permission::WorldWritable{});

    // Then allocation fails because FindNextFreeOffset exhausts available space
    EXPECT_FALSE(result.has_value());
}

#endif  // defined(__QNXNTO__)

TEST(IvshmemTypedMemoryProviderStaticTest, DirectorySizeIs4096)
{
    // When checking directory size constant
    // Then it equals 4096 bytes
    EXPECT_EQ(IvshmemTypedMemoryProvider::kDirectorySize, 4096U);
}

TEST(IvshmemTypedMemoryProviderStaticTest, MaxDirectoryEntriesFitsInDirectoryPage)
{
    // When calculating max directory entries
    const std::uint32_t expected =
        (4096U - IvshmemTypedMemoryProvider::kDirectoryHeaderSize) / sizeof(IvshmemTypedMemoryProvider::DirectoryEntry);

    // Then count fits within directory page and is positive
    EXPECT_EQ(IvshmemTypedMemoryProvider::kMaxDirectoryEntries, expected);
    EXPECT_GT(IvshmemTypedMemoryProvider::kMaxDirectoryEntries, 0U);
}

TEST(IvshmemTypedMemoryProviderStaticTest, DirectoryEntryHasExpectedSize)
{
    // When checking DirectoryEntry struct size
    // Then size equals 72 bytes: 4 (name_hash) + 4 (alloc_size) + 8 (bar_offset) + 56 (name)
    EXPECT_EQ(sizeof(IvshmemTypedMemoryProvider::DirectoryEntry), 72U);
}

}  // namespace

}  // namespace score::mw::com::gateway::qemu::ivshmem
