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
#ifndef BASELIBS_SCORE_MEMORY_PMR_RING_BUFFER_H
#define BASELIBS_SCORE_MEMORY_PMR_RING_BUFFER_H

#include "score/vector.hpp"

#include <type_traits>

namespace score::memory
{

/// \brief PMR-aware initialization-time-sized ring buffer queue.
/// \details This class is based on the ideas from score::cpp::circular_buffer implementation (parts of the source code
///          were copied from there), but is supposed to provide initialization-time buffer capacity.
///          Only the minimum queue-like functionality is currently implemented.
///
/// \tparam T Specifies the value type of a single element.
///           The element type itself can be PMR-aware; use of monotonic_buffer_resource in such a case is not advised
///           due to inherent buffer space leak.
template <typename T>
class PmrRingBuffer
{
  public:
    using allocator_type = score::cpp::pmr::polymorphic_allocator<PmrRingBuffer>;
    using value_type = T;
    using size_type = std::size_t;

    /// \brief Constructs an empty ring buffer with a given capacity, using a given PMR allocator
    /// \param capacity A maximum number of values in the queue (greater than zero)
    /// \param allocator The allocator bound to the memory resource used
    explicit PmrRingBuffer(size_type capacity, const allocator_type& allocator)
        : array_(capacity, allocator), tail_{0U}, head_{0U}, size_{0U}
    {
    }

    /// \brief Clears all elements from the container.
    ~PmrRingBuffer()
    {
        clear();
    }

    /// \brief Removes all elements from the container.
    void clear()
    {
        const size_type number_of_elements_to_remove{size()};
        for (size_type i{0U}; i < number_of_elements_to_remove; ++i)
        {
            pop_front();
        }
    }

    /// \brief Removes the first element in the container.
    void pop_front()
    {
        if (!empty())
        {
            allocator_type allocator{array_.get_allocator()};
            std::allocator_traits<allocator_type>::destroy(allocator, &front());
            ++head_;
            head_ %= capacity();
            --size_;
        }
    }

    /// \brief Returns the first element in the (const) container.
    ///
    /// \return Reference to the first element in the container.
    const T& front() const
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(!empty());
        return *p_at(head_);
    }

    /// \brief Returns the first element in the (non-const) container.
    ///
    /// \return Reference to the first element in the container.
    T& front()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG(!empty());
        return *p_at(head_);
    }

    /// \brief constructs a new element in place at the end of the buffer
    ///
    /// It will remove the element at front() if the buffer is full
    ///
    /// \param arguments The arguments from which the element is constructed
    template <typename... Ts>
    void emplace_back(Ts&&... arguments)
    {
        if (full())
        {
            pop_front();
        }
        allocator_type allocator{array_.get_allocator()};
        std::allocator_traits<allocator_type>::construct(allocator, p_at(tail_), std::forward<Ts>(arguments)...);
        ++tail_;
        tail_ %= capacity();
        ++size_;
    }

    /// \brief Checks if container contains elements.
    ///
    /// \return True, if the container does not contain elements - false otherwise.
    bool empty() const
    {
        return (0U == size());
    }

    /// \brief Returns true if container is full.
    ///
    /// \return True if container is full - otherwise false.
    bool full() const
    {
        return (capacity() == size());
    }

    /// \brief Returns the number elements in the container.
    ///
    /// \return The number of elements in the container.
    size_type size() const
    {
        return size_;
    }

    /// \brief Returns the maximum size of the circular buffer.
    ///
    /// \return Maximum size of the circular buffer.
    size_type capacity() const
    {
        return array_.size();
    }

  private:
    PmrRingBuffer(const PmrRingBuffer& other) = delete;
    PmrRingBuffer(PmrRingBuffer&& other) = delete;
    PmrRingBuffer& operator=(const PmrRingBuffer& rhs) = delete;
    PmrRingBuffer& operator=(PmrRingBuffer&& rhs) = delete;

    /// \brief Returns const pointer to the element of the internal array.
    ///
    /// \param pos The (zero-based) position in the array
    // Suppress "AUTOSAR C++14 A5-2-4" rule finding: "reinterpret_cast shall not be used.".
    // Cast from std::aligned_storage (opposed to void*) needs
    // reinterpret_cast. reinterpret_cast is fine here as we obey case/rule 5 under
    // https://en.cppreference.com/w/cpp/language/reinterpret_cast! Alignment needs of the cast to type T are
    // fulfilled as the aligned storage represented by array_ has ben exactly aligned to T's needs.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): Tolerated since performed cast std::aligned_storage
    // coverity[autosar_cpp14_a5_2_4_violation]
    const T* p_at(size_type pos) const
    {
        return reinterpret_cast<const T*>(&array_[pos]);
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast): see above

    /// \brief Returns pointer to the element of the internal array.
    ///
    /// \param pos The (zero-based) position in the array
    /* Cast from std::aligned_storage (opposed to void*) needs
     * reinterpret_cast. Further explanation see above. */
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): Tolerated since performed cast std::aligned_storage
    // coverity[autosar_cpp14_a5_2_4_violation]
    T* p_at(size_type pos)
    {
        return reinterpret_cast<T*>(&array_[pos]);
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast): see above

    /// \brief Internal data array
    score::cpp::pmr::vector<typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type> array_;

    /// \brief Index of the last valid data in the circular buffer.
    size_type tail_;

    /// \brief Index of the first valid data in the circular buffer.
    size_type head_;

    /// \brief Amount of valid elements in the circular buffer.
    size_type size_;
};

}  // namespace score::memory

#endif  // BASELIBS_SCORE_MEMORY_PMR_RING_BUFFER_H
