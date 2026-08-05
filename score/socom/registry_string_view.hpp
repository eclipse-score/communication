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

#ifndef SRC_SOCOM_INCLUDE_SCORE_SOCOM_REGISTRY_STRING_VIEW
#define SRC_SOCOM_INCLUDE_SCORE_SOCOM_REGISTRY_STRING_VIEW

#include <ostream>
#include <string_view>

namespace score::socom {

///
/// \class Registry_string_view
///
/// \brief Registry_string_view is an unmodifiable view of a string held in String_registry.
///        For performance reasons the comparison between instances of Registry_string_view is done
///        using simple pointer equality check and a check against other String_views is done by
///        character comparison.
///        It is the programmer's responsibility to ensure that Registry_string_view does not
///        outlive the String_registry it is referring to.
///
class Registry_string_view final {
    friend class String_registry;

   public:
    using const_pointer = typename std::string_view::const_pointer;
    using const_iterator = typename std::string_view::const_iterator;
    using difference_type = typename std::string_view::difference_type;
    using size_type = typename std::string_view::size_type;
    // npos is a constant representing the largest possible value of size_type, because of
    // signed-to-unsigned implicit conversion. It is used as an "end of string" or "not found"
    // indicator. More details: https://en.cppreference.com/w/cpp/string/basic_string/npos
    static constexpr size_type npos = static_cast<size_type>(-1);

    ///
    /// \brief Copy constructor (default)
    ///
    constexpr Registry_string_view(Registry_string_view const& other) noexcept = default;

    ///
    /// \brief Move constructor (default)
    ///
    constexpr Registry_string_view(Registry_string_view&& other) noexcept = default;

    ///
    /// \brief Destructor (default)
    ///
    ~Registry_string_view() noexcept = default;

    ///
    /// \brief Copy assignment operator (default)
    ///
    constexpr Registry_string_view& operator=(Registry_string_view const& view) & noexcept =
        default;

    ///
    /// \brief Move assignment operator (default)
    ///
    constexpr Registry_string_view& operator=(Registry_string_view&& view) & noexcept = default;

    ///
    /// \brief Get string data
    ///
    constexpr const_pointer data() const noexcept { return m_string_view.data(); }

    ///
    /// \brief Get string length
    ///
    constexpr size_type length() const noexcept { return m_string_view.length(); }

    ///
    /// \brief Get string length
    ///
    constexpr size_type size() const noexcept { return m_string_view.size(); }

    ///
    /// \brief Get whether the string is empty
    ///
    constexpr bool empty() const noexcept { return m_string_view.empty(); }

    ///
    /// \brief Const iterator to the beginning
    ///
    constexpr const_iterator begin() const noexcept { return data(); }

    ///
    /// \brief Const iterator to the beginning
    ///
    constexpr const_iterator cbegin() const noexcept { return data(); }

    ///
    /// \brief Iterator to end
    ///
    const_iterator end() const noexcept {
        return std::next(data(), static_cast<difference_type>(length()));
    }

    ///
    /// \brief Const iterator to the end
    ///
    const_iterator cend() const noexcept {
        return std::next(data(), static_cast<difference_type>(length()));
    }

    constexpr std::string_view string_view() const noexcept { return m_string_view; }

   private:
    std::string_view m_string_view;

    explicit constexpr Registry_string_view(std::string_view view)
        : m_string_view{std::move(view)} {}
};

///
/// \brief operator==
///
constexpr bool operator==(Registry_string_view lhs, Registry_string_view rhs) noexcept {
    return (lhs.data() == rhs.data()) && (lhs.length() == rhs.length());
}

///
/// \brief operator!=
///
constexpr bool operator!=(Registry_string_view lhs, Registry_string_view rhs) noexcept {
    return !(lhs == rhs);
}

///
/// \brief operator<
///
constexpr bool operator<(Registry_string_view lhs, Registry_string_view rhs) noexcept {
    return lhs.string_view() < rhs.string_view();
}

///
/// \brief operator<=
///
constexpr bool operator<=(Registry_string_view lhs, Registry_string_view rhs) noexcept {
    return lhs.string_view() <= rhs.string_view();
}

///
/// \brief operator>
///
constexpr bool operator>(Registry_string_view lhs, Registry_string_view rhs) noexcept {
    return lhs.string_view() > rhs.string_view();
}

///
/// \brief operator>=
///
constexpr bool operator>=(Registry_string_view lhs, Registry_string_view rhs) noexcept {
    return lhs.string_view() >= rhs.string_view();
}

///
/// \brief operator<<
///        Overload of << to write the string from a Registry_string_view to ostream.
///
/// \return Reference to the ostream object used for writing, to enable chaining of << operations
///         as usual.
///
inline std::ostream& operator<<(std::ostream& os, Registry_string_view v) {
    return os.write(v.data(), static_cast<std::streamsize>(v.length()));
}

}  // namespace score::socom

namespace std {

/// \brief std::hash specialization for Registry_string_view
///
/// \return Hash value for the given Registry_string_view
///
template <>
struct hash<::score::socom::Registry_string_view> {
    size_t operator()(::score::socom::Registry_string_view const& sv) const noexcept {
        // For the conversion of Registry_string_view data pointer to intptr_t, the reinterpret_cast
        // is necessary.
        // Pointer-to-integer conversion is required for memory address hashing in  this case.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return std::hash<std::intptr_t>{}(reinterpret_cast<std::intptr_t>(sv.data()));
    }
};

}  // namespace std

#endif  // SRC_SOCOM_INCLUDE_SCORE_SOCOM_REGISTRY_STRING_VIEW
