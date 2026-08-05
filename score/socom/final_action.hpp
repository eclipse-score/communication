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

#ifndef SCORE_SOCOM_FINAL_ACTION_HPP
#define SCORE_SOCOM_FINAL_ACTION_HPP

#include <algorithm>
#include <score/move_only_function.hpp>

namespace score {
namespace socom {

///
/// \class Final_action
///
/// \brief Wraps a functor that shall be executed only when an instance of this class gets destroyed
///
class Final_action {
   public:
    using F = score::cpp::move_only_function<void(), 128U>;

    ///
    /// \brief Constructor
    /// \param f functor to be called
    ///
    explicit Final_action(F f) noexcept : m_f{std::move(f)} {}

    ///
    /// \brief Move constructor
    ///
    Final_action(Final_action&& other) noexcept : m_f{std::move(other.m_f)} {
        // Reset it always to get consistent behavior.
        other.m_f = nullptr;
    }

    Final_action(Final_action const&) = delete;
    Final_action& operator=(Final_action const&) = delete;
    Final_action& operator=(Final_action&&) = delete;

    ///
    /// \brief Destructor
    ///
    ~Final_action() noexcept { execute(); }

    ///
    /// \brief Runs the functor and disarms the Final_action. It will destroy the stored functor.
    ///
    void execute() noexcept {
        F tmp_f = nullptr;
        std::swap(tmp_f, m_f);
        try {
            if (!tmp_f.empty()) {
                tmp_f();
            }
        } catch (...) {
        }
    }

   private:
    F m_f;
};

}  // namespace socom
}  // namespace score

#endif  // SCORE_SOCOM_FINAL_ACTION_HPP
