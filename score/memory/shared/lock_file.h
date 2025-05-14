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
#ifndef SCORE_LIB_MEMORY_SHARED_LOCK_FILE_H
#define SCORE_LIB_MEMORY_SHARED_LOCK_FILE_H

#include <cstdint>
#include <optional>
#include <string>

namespace score::memory::shared
{

class FlockMutex;

/// \brief RAII style class which manages a lock file in the file system
///
/// The LockFile object can be created with Create, CreateOrOpen or Open. Creating a LockFile with Create will "own" the
/// file while CreateOrOpen or Open will not. If the LockFile "owns" the file, then the path will be closed and unlinked
/// on destruction of the LockFile object. If the LockFile does not "own" the file, then other LockFile objects can
/// still open the path until the LockFile which "owns" the file is destroyed. Ownership can also be explicitly taken by
/// calling TakeOwnership.
class LockFile
{
    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used.".
    // The 'friend' class is employed to encapsulate non-public members.
    // This design choice protects end users from implementation details
    // and prevents incorrect usage. Friend classes provide controlled
    // access to private members, utilized internally, ensuring that
    // end users cannot access implementation specifics.
    // coverity[autosar_cpp14_a11_3_1_violation : FALSE]
    friend FlockMutex;

  public:
    static std::optional<LockFile> Create(std::string path) noexcept;
    static std::optional<LockFile> CreateOrOpen(std::string path, bool take_ownership) noexcept;
    static std::optional<LockFile> Open(std::string path) noexcept;

    void TakeOwnership() noexcept
    {
        lock_file_owns_file_ = true;
    }

    ~LockFile() noexcept;

    LockFile(const LockFile&) = delete;
    LockFile& operator=(const LockFile&) = delete;

    LockFile(LockFile&& other) noexcept;
    LockFile& operator=(LockFile&& other) & noexcept;

  private:
    LockFile(std::string path, const std::int32_t file_descriptor, bool owns_file) noexcept;

    void CleanupFile() noexcept;

    std::string lock_file_path_;
    std::int32_t file_descriptor_;

    bool lock_file_owns_file_;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_LOCK_FILE_H
