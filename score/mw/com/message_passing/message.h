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
#ifndef SCORE_MW_COM_MESSAGE_PASSING_SHORT_MESSAGE_H
#define SCORE_MW_COM_MESSAGE_PASSING_SHORT_MESSAGE_H

#include <array>
#include <cstdint>
#include <type_traits>

namespace score::mw::com::message_passing
{

/// \brief Identifies a message
using MessageId = std::int8_t;

/// \brief Represents the payload that is transmitted via a ShortMessage
using ShortMessagePayload = std::uint64_t;

/// \brief Represents the payload that is transmitted via a MediumMessage
using MediumMessagePayload = std::array<uint8_t, 16>;

static_assert(std::is_trivially_copyable<pid_t>::value, "pid_t must be TriviallyCopyable");
static_assert(std::is_trivially_copyable<MessageId>::value, "MessageId must be TriviallyCopyable");
static_assert(std::is_trivially_copyable<ShortMessagePayload>::value, "ShortMessagePayload must be TriviallyCopyable");
static_assert(std::is_trivially_copyable<MediumMessagePayload>::value,
              "MediumMessagePayload must be TriviallyCopyable");

/// \brief Base class of all messages, containing common members.
///
/// \details The pid value depends on the context. If a message is sent, it shall contain the PID of the target process.
/// I.e. the caller of Sender::Send() shall fill in the PID before. In case of reception of a message, the Receiver
/// shall fill in the PID from where he received the message, before calling the registered handler.
///
/// It depends on the OS specific implementation/optimization of Sender/Receiver, whether the PID has to be transmitted
/// explicitly (therefore extending the payload) or whether it is transmitted implicitly. E.g., when using QNX messaging
/// mechanisms, the PID gets transmitted implicitly!
/// In case a message gets received
// Suppress "AUTOSAR C++14 A12-8-6" rule finding. This rule states: "Copy and move constructors and copy assignment and
// move assignment operators shall be declared protected or defined ""=delete"" in base class."
// The rule is intended to protect against slicing. BaseMessage class is created for layout documentation purposes
// and is not used in the contexts where slicing is possible: there are no final objects, pointers or references of
// this type. Trying to follow the rule will produce source bloat in this and derived classes with no practical benefit.
// coverity[autosar_cpp14_a12_8_6_violation]
class BaseMessage
{
  public:
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // In MISRA C++:2023 this is no longer an issue and it is covered by rule 14.1.1
    // coverity[autosar_cpp14_m11_0_1_violation]
    MessageId id{};
    // coverity[autosar_cpp14_m11_0_1_violation]
    pid_t pid{-1};
};

/// \brief A ShortMessage shall be used for Inter Process Communication that is Asynchronous and acts as control
/// mechanism.
///
/// \details Different operating systems can implement short messages very efficiently. This is given by the fact that
/// such messages fit into register spaces on the CPU and copying the information is highly efficient. On QNX this is
/// for example the case with Pulses. By providing such an interface, we make it easy for our applications to exchange
/// data as efficient as possible. It shall be noted that no real data shall be transferred over this communication
/// method, it shall rather act as a way to control or notify another process. The efficiency is gained by not providing
/// strong typing, meaning the payload needs to be serialized by the user of the API. If you are searching for a strong
/// typed interface, please consider our mw::com implementation (or ARA::COM).
class ShortMessage : public BaseMessage
{
  public:
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // In MISRA C++:2023 this is no longer an issue and it is covered by rule 14.1.1
    // coverity[autosar_cpp14_m11_0_1_violation]
    ShortMessagePayload payload{};
};

static_assert(std::is_trivially_copyable<ShortMessage>::value, "ShortMessage type must be TriviallyCopyable");

/// \brief A MediumMessage shall be used for Inter Process Communication that is Asynchronous and acts as control
/// mechanism.
///
/// \details Opposed to short messages, the medium size message might not be implemented that efficiently on various OS.
/// Its size being double that of short-messages might hinder solutions, where message payload gets exchanged only via
/// registers after a context switch. Still the payload size is small enough, that no (heap) memory allocation takes
/// place.
/// Introduction of medium messages was driven by some LoLa needs, where short messages weren't sufficient.
class MediumMessage : public BaseMessage
{
  public:
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to be organized into a coherent organized data structure.
    // In MISRA C++:2023 this is no longer an issue and it is covered by rule 14.1.1
    // coverity[autosar_cpp14_m11_0_1_violation]
    MediumMessagePayload payload{};
};

static_assert(std::is_trivially_copyable<MediumMessage>::value, "MediumMessage type must be TriviallyCopyable");

}  // namespace score::mw::com::message_passing

#endif  // SCORE_MW_COM_MESSAGE_PASSING_SHORT_MESSAGE_H
