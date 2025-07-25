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
#ifndef SCORE_MW_COM_IMPL_FINDSERVICEHANDLE_H
#define SCORE_MW_COM_IMPL_FINDSERVICEHANDLE_H

#include <cstddef>
#include <ostream>

namespace score::mw
{

namespace log
{
class LogStream;
}

namespace com::impl
{

class FindServiceHandle;
class FindServiceHandleView;

/**
 * \brief A FindServiceHandle is returned by any StartFindService() method and is
 * used to identify different searches. It needs to be passed to StopFindService()
 * in order to cancel a respective search.
 *
 * \requirement SWS_CM_00303
 */
class FindServiceHandle final
{
  public:
    FindServiceHandle() = delete;
    ~FindServiceHandle() noexcept = default;

    /**
     * \brief CopyAssignment for FindServiceHandle
     *
     * \post *this == other
     * \param other The FindServiceHandle *this shall be constructed from
     * \return The FindServiceHandle that was constructed
     */
    FindServiceHandle& operator=(const FindServiceHandle& other) = default;
    FindServiceHandle(const FindServiceHandle& other) = default;
    FindServiceHandle(FindServiceHandle&& other) noexcept = default;
    FindServiceHandle& operator=(FindServiceHandle&& other) noexcept = default;

    /**
     * \brief Compares two instances for equality
     *
     * \param lhs The first instance to check for equality
     * \param rhs The second instance to check for equality
     * \return true if lhs and rhs equal, false otherwise
     */
    friend bool operator==(const FindServiceHandle& lhs, const FindServiceHandle& rhs) noexcept;

    /**
     * \brief LessThanComparable operator
     *
     * \param lhs The first FindServiceHandle instance to compare
     * \param rhs The second FindServiceHandle instance to compare
     * \return true if lhs is less then rhs, false otherwise
     */
    friend bool operator<(const FindServiceHandle& lhs, const FindServiceHandle& rhs) noexcept;

    friend mw::log::LogStream& operator<<(mw::log::LogStream& log_stream, const FindServiceHandle& find_service_handle);

    friend std::ostream& operator<<(std::ostream& ostream_out, const FindServiceHandle& find_service_handle);

  private:
    std::size_t uid_;

    explicit FindServiceHandle(const std::size_t uid);

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Suppress "AUTOSAR C++14 M3-2-3", The rule states: "A type, object or function that is used in multiple
    // translation units shall be declared in one and only one file."
    // Design decision: Friend class required to access private constructor. This pattern violates both above rules.
    // However, it is necessery to hide implementation details.
    // coverity[autosar_cpp14_m3_2_3_violation]
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend FindServiceHandle make_FindServiceHandle(const std::size_t uid);

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend FindServiceHandleView;
};

/**
 * \brief A make_ function is introduced to hide the Constructor of FindServiceHandle.
 * The FindServiceHandle will be exposed to the API user and by not having a public constructor
 * we can avoid that by chance the user will construct this class. Introducing a custom make method
 * that is _not_ mentioned in the standard, will avoid this!
 *
 * \param uid The unique identifier for a FindServiceHandle
 * \return A constructed FindServiceHandle
 */
FindServiceHandle make_FindServiceHandle(const std::size_t uid);

/**
 * \brief The score::mw::com::FindServiceHandle API is described by the ara::com standard.
 * But we also need to use it for internal purposes, because we need to access some state information
 * that is not exposed by the public API described in the adaptive AUTOSAR Standard.
 * In order to not leak implementation details, we come up with a `View` onto the FindServiceHandle.
 * Since our view is anyhow _only_ located in the `impl` namespace, there is zero probability that
 * any well minded user would depend on it.
 */
class FindServiceHandleView
{
  public:
    constexpr explicit FindServiceHandleView(const FindServiceHandle& handle) : handle_{handle} {}

    constexpr std::size_t getUid() const
    {
        return handle_.uid_;
    }

  private:
    const FindServiceHandle& handle_;
};

}  // namespace com::impl
}  // namespace score::mw

namespace std
{
template <>
class hash<score::mw::com::impl::FindServiceHandle>
{
  public:
    std::size_t operator()(const score::mw::com::impl::FindServiceHandle& find_service_handle) const noexcept;
};
}  // namespace std

#endif  // SCORE_MW_COM_IMPL_FINDSERVICEHANDLE_H
