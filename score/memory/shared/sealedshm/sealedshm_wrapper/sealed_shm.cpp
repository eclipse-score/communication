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
#include "score/memory/shared/sealedshm/sealedshm_wrapper/sealed_shm.h"
#if defined __QNX__
#include "score/os/qnx/mman_impl.h"
#include <fcntl.h>
#else
#include <tuple>
#endif

namespace score::memory::shared
{

ISealedShm* SealedShm::mock_{nullptr};

// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Need to limit this functionality to QNX only
// coverity[autosar_cpp14_a16_0_1_violation]
#if defined __QNX__
SealedShm::SealedShm(std::unique_ptr<const os::qnx::MmanQnx> mman) noexcept : ISealedShm{}, mman_{std::move(mman)} {}
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#endif

auto SealedShm::OpenAnonymous(const mode_t mode) noexcept -> score::cpp::expected<std::int32_t, score::os::Error>
{
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#if defined __QNX__
    // Suppress "AUTOSAR C++14 M5-2-8" rule finding: "An object with integer type or pointer to void type shall not be
    // converted to an object with pointer type".
    // This rule fires because SHM_ANON is defined in QNX as ((char*)-1). Usage of SHM_ANON is ok because it is defined
    // by the QNX API.
    // Suppress "AUTOSAR C++14 M5-0-21" rule finding: "Bitwise operators shall only be applied to operands of unsigned
    // underlying type.".
    // No harm to do bitwise operations on these values. The values are defined by the QNX API as signed integers.
    // Suppress "AUTOSAR C++14 A5-2-2 finding: "Traditional C-style casts shall not be used.".
    // Suppress "AUTOSAR C++14 M5-2-9" rule finding: "A cast shall not convert a pointer type to an integral type."
    // Cast is happening outside our code domain
    // coverity[autosar_cpp14_m5_2_8_violation]
    // coverity[autosar_cpp14_m5_0_21_violation]
    // coverity[autosar_cpp14_a5_2_2_violation]
    // coverity[autosar_cpp14_m5_2_9_violation]
    return mman_->shm_open(SHM_ANON, O_RDWR | O_CREAT | O_ANON, mode);
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#else
    std::ignore = mode;
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOTSUP));
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#endif
}

auto SealedShm::Seal(int fd, std::uint64_t size) noexcept -> score::cpp::expected_blank<score::os::Error>
{
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#if defined __QNX__
    // Suppress "AUTOSAR C++14 M5-0-21" rule finding: "Bitwise operators shall only be applied to operands of unsigned
    // underlying type.".
    // No harm to do bitwise operations on these values. The values are defined by the QNX API.
    // coverity[autosar_cpp14_m5_0_21_violation]
    const auto result = mman_->shm_ctl(fd, SHMCTL_ANON | SHMCTL_SEAL, 0UL, size);
    if (result.has_value())
    {
        return {};
    }
    else
    {
        return score::cpp::make_unexpected(result.error());
    }
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#else
    std::ignore = fd;
    std::ignore = size;
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOTSUP));
// coverity[autosar_cpp14_a16_0_1_violation] Need to limit this functionality to QNX only
#endif
}

auto SealedShm::InjectMock(ISealedShm* mock) noexcept -> void
{
    mock_ = mock;
}

auto SealedShm::instance() noexcept -> ISealedShm&
{
    if (mock_ != nullptr)
    {
        return *mock_;
    }

    // Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
    // constant-initialized.".
    // Documentation and example for Rule A3-3-2 in
    // https://www.autosar.org/fileadmin/standards/R20-11/AP/AUTOSAR_RS_CPP14Guidelines.pdf show that SealedShm would
    // need a constexpr constructor to be compliant. This is not possible because here because the parameter of type
    // unique_ptr doesn't have a constexpr constructor in C++17.
    // coverity[autosar_cpp14_a3_3_2_violation]
    static SealedShm instance{};
    return instance;
}

}  // namespace score::memory::shared
