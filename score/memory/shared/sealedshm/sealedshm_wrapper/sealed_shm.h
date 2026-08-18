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
#ifndef SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_SEALED_SHM_H
#define SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_SEALED_SHM_H

#include "score/expected.hpp"
#include "score/memory/shared/sealedshm/sealedshm_wrapper/i_sealed_shm.h"
#include "score/os/errno.h"
#if defined __QNX__
#include "score/os/qnx/mman.h"
#include "score/os/qnx/mman_impl.h"
#endif
#include <cstdint>

namespace score::memory::shared
{
namespace test
{
class SealedShmTestAttorney;
}

class SealedShm : public ISealedShm
{
  public:
    ~SealedShm() override = default;
    SealedShm(const SealedShm&) noexcept = delete;
    SealedShm(SealedShm&&) noexcept = default;
    SealedShm& operator=(const SealedShm&) & noexcept = delete;
    SealedShm& operator=(SealedShm&&) & noexcept = default;

    /**
     * @brief Create and open an anonymous shared memory object.
     * @param mode the permissions for the shared memory object.
     * @return filedescriptor on success, score::os::Error on error.
     */
    score::cpp::expected<std::int32_t, score::os::Error> OpenAnonymous(const mode_t mode) noexcept override;

    /**
     * @brief Seal the given shared memory object.
     * @param fd A file descriptor referencing the shared memory object to be sealed.
     * @param size The fixed size for the sealed shared memory object.
     * @return score::cpp::blank on success, score::os::Error on error.
     */
    score::cpp::expected_blank<score::os::Error> Seal(int fd, std::uint64_t size) noexcept override;

    /**
     * @brief Injects mock.
     * @param mock pointer to a Mock object. If it is not nullptr all calls are redirected to provided object.
     */
    static void InjectMock(ISealedShm* mock) noexcept;

    static ISealedShm& instance() noexcept;

  private:
    // Suppress "AUTOSAR C++14 A11-3-1" rule finding: "Friend declarations shall not be used.".
    // Friend class is used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class test::SealedShmTestAttorney;
// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Need to limit this functionality to QNX only
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined __QNX__
    explicit SealedShm(
        std::unique_ptr<const os::qnx::MmanQnx> mman = std::make_unique<const os::qnx::MmanQnxImpl>()) noexcept;
    std::unique_ptr<const os::qnx::MmanQnx> mman_;
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#else
    SealedShm() noexcept = default;
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#endif

    static ISealedShm* mock_;
};

}  // namespace score::memory::shared

#endif  // SCORE_LIB_MEMORY_SHARED_SEALEDSHM_SEALEDSHM_WRAPPER_SEALED_SHM_H
