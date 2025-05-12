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
#ifndef SCORE_MW_COM_IMPL_SAMPLE_PTR_REFERENCE_TRACKER_H
#define SCORE_MW_COM_IMPL_SAMPLE_PTR_REFERENCE_TRACKER_H

#include <atomic>
#include <optional>
#include <type_traits>

namespace score::mw::com::impl
{

class SampleReferenceGuard;
class SampleReferenceTracker;

/// Reservation of a number of reference counts. Hands out guards for single references and returns unused references on
/// destruction to its associated sample tracker.
class TrackerGuardFactory final
{

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // The following friend function is only visible in this translation unit and is completely hidden from the
    // user. This use of the friend keyword allows us to hide the constructor function from the end user, thus
    // making this class better encapsulated. Therefore, we find this use of the friend keyword acceptable.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend TrackerGuardFactory makeTrakerGuardFactory(SampleReferenceTracker& tracker,
                                                      const std::size_t num_available_guards) noexcept;

  public:
    TrackerGuardFactory(TrackerGuardFactory&&) noexcept;
    TrackerGuardFactory(const TrackerGuardFactory&) = delete;
    TrackerGuardFactory& operator=(TrackerGuardFactory&&) = delete;
    TrackerGuardFactory& operator=(const TrackerGuardFactory&) = delete;
    ~TrackerGuardFactory() noexcept;

    /// Returns the number of available guards, i.e. the number of times TakeGuard() can be called without getting an
    /// score::cpp::nullopt.
    ///
    /// \return Number of guards available.
    std::size_t GetNumAvailableGuards() const noexcept;

    /// Creates one SampleReferenceGuard, reducing the number of reserved references by one. If no references are left,
    /// it will return score::cpp::nullopt.
    ///
    /// \return A guard managing a single reference, std::nullopt otherwise.
    std::optional<SampleReferenceGuard> TakeGuard() noexcept;

  private:
    TrackerGuardFactory(SampleReferenceTracker& tracker, const std::size_t num_available_guards) noexcept;

    /// Associated tracker
    SampleReferenceTracker& tracker_;
    /// Number of reserved references
    std::size_t num_available_guards_;
};

/// Implements thread safe reference counting by handing out a factory class that emits guards that will, on
/// destruction, hand back a reference to the instance of this class it was created from.
class SampleReferenceTracker final
{

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // The following friend function is only visible in this translation unit and is completely hidden from the
    // user. This use of the friend keyword allows us to hide the constructor function from the end user, thus
    // making this class better encapsulated. Therefore, we find this use of the friend keyword acceptable.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend void deallocateSampleReferenceTracker(SampleReferenceTracker& instance,
                                                 const std::size_t num_deallocations) noexcept;

  public:
    /// Creates an uninitialized instance of the class. In this state, the only valid operation is calling Reset().
    SampleReferenceTracker() = default;
    /// Creates an initialized instance of the class.
    /// \param max_num_samples Number of samples that are allowed.
    explicit SampleReferenceTracker(const std::size_t max_num_samples) noexcept;

    SampleReferenceTracker(SampleReferenceTracker&&) = delete;
    SampleReferenceTracker(const SampleReferenceTracker&) = delete;
    SampleReferenceTracker& operator=(SampleReferenceTracker&&) = delete;
    SampleReferenceTracker& operator=(const SampleReferenceTracker&) = delete;

    ~SampleReferenceTracker() noexcept = default;

    /// Get the number of available samples, i.e. the maximum number that a call to Allocate() will reserve.
    ///
    /// Since the class is meant to be used in a multithreading environment, this numer is only a snapshot and may
    /// change at any point in time.
    ///
    /// \return Number of available samples.
    std::size_t GetNumAvailableSamples() const noexcept;

    /// Tells whether any samples are currently in use.
    ///
    /// Since the class is meant to be used in a multithreading environment, this information may change at any time.
    /// \return true if any guards or trackers are instantiated, false otherwise.
    bool IsUsed() const noexcept;

    /// Allocates a number of samples for later use by providing instances of SampleReferenceGuard.
    ///
    /// The allocated samples can then be acquired by calling TrackerGuardFactory::TakeGuard of the returned instance.
    /// The actual number of allocated samples may be lower (including 0) than the requested number.
    ///
    /// \param num_samples Maximum number of samples to preallocate.
    /// \return A factory that will create guards for single samples.
    TrackerGuardFactory Allocate(std::size_t num_samples) noexcept;

    /// Reinitialize this instance with a new number of maximum samples.
    ///
    /// The caller needs to ensure that IsUsed() is permanently false, otherwise a call to this method may lead to
    /// unexpected behavior since other threads may have allocated or reinitialized in parallel.
    ///
    /// \param max_num_samples Limit on the numer of samples to be handed out.
    void Reset(const std::size_t max_num_samples) noexcept;

  private:
    void Deallocate(const std::size_t num_deallocations) noexcept;

    std::atomic_size_t available_samples_;
    std::size_t max_num_samples_;
};

/// Tracks the usage of a single allocated sample and releases the associated reference on destruction.
class SampleReferenceGuard final
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // The following friend function is only visible in this translation unit and is completely hidden from the
    // user. This use of the friend keyword allows us to hide the constructor function from the end user, thus
    // making this class better encapsulated. Therefore, we find this use of the friend keyword acceptable.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend SampleReferenceGuard makeSampleReferenceGuard(SampleReferenceTracker& tracker) noexcept;

  public:
    SampleReferenceGuard() = default;
    SampleReferenceGuard(const SampleReferenceGuard&) = delete;
    SampleReferenceGuard& operator=(const SampleReferenceGuard&) = delete;
    SampleReferenceGuard(SampleReferenceGuard&&) noexcept;
    SampleReferenceGuard& operator=(SampleReferenceGuard&&) noexcept;
    ~SampleReferenceGuard() noexcept;

  private:
    explicit SampleReferenceGuard(SampleReferenceTracker& tracker) noexcept;
    void Deallocate() noexcept;

    /// Reference to the tracker. It must not move, otherwise the pointer will be dangling.
    SampleReferenceTracker* tracker_{nullptr};
    static_assert((std::is_move_assignable<SampleReferenceTracker>::value == false) &&
                      (std::is_move_constructible<SampleReferenceTracker>::value == false),
                  "SampleReferenceTracker must not be movable, otherwise the reference guard class may fail to work");
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SAMPLE_PTR_REFERENCE_TRACKER_H
