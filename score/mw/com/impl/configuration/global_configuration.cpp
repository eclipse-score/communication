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
#include "score/mw/com/impl/configuration/global_configuration.h"

#include "score/mw/log/logging.h"

#include <exception>

namespace score::mw::com::impl
{

GlobalConfiguration::GlobalConfiguration() noexcept
    : process_asil_level_{QualityType::kASIL_QM},
      message_rx_queue_size_qm{DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE},
      message_rx_queue_size_b{DEFAULT_MIN_NUM_MESSAGES_RX_QUEUE},
      message_tx_queue_size_b{DEFAULT_MIN_NUM_MESSAGES_TX_QUEUE},
      shm_size_calc_mode_{ShmSizeCalculationMode::kSimulation}
{
}

std::int32_t GlobalConfiguration::GetReceiverMessageQueueSize(const QualityType quality_type) const noexcept
{
    // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: An unconditional throw or break
    // statement shall terminate every nonempty switch-clause." and "A switch statement shall be a well-formed switch
    // statement.", respectively.The `return` and std::terminate() statements in this case clause unconditionally exits
    // the function, making an additional `break` statement redundant.
    // coverity[autosar_cpp14_m6_4_3_violation] See above
    switch (quality_type)
    {
        // coverity[autosar_cpp14_m6_4_5_violation] std::terminate will terminate this switch clause
        case QualityType::kInvalid:
            ::score::mw::log::LogFatal("lola") << "Invalid ASIL in global/asil-level, terminating.";
            std::terminate();
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
        case QualityType::kASIL_QM:
            return message_rx_queue_size_qm;
        // coverity[autosar_cpp14_m6_4_5_violation] Return will terminate this switch clause
        case QualityType::kASIL_B:
            return message_rx_queue_size_b;
        // coverity[autosar_cpp14_m6_4_5_violation] std::terminate will terminate this switch clause
        default:
            ::score::mw::log::LogFatal("lola") << "Unknown ASIL parsed from config, terminating";
            std::terminate();
    }
}

void GlobalConfiguration::SetReceiverMessageQueueSize(const QualityType quality_type,
                                                      const std::int32_t queue_size) noexcept
{
    // Suppress "AUTOSAR C++14 M6-4-5" and "AUTOSAR C++14 M6-4-3", The rule states: An unconditional throw or break
    // statement shall terminate every nonempty switch-clause." and "A switch statement shall be a well-formed switch
    // statement.", respectively.The `return` and std::terminate() statements in this case clause unconditionally exits
    // the function, making an additional `break` statement redundant.
    // coverity[autosar_cpp14_m6_4_3_violation] See above
    switch (quality_type)
    {
        // coverity[autosar_cpp14_m6_4_5_violation] std::terminate will terminate this switch clause
        case QualityType::kInvalid:
            ::score::mw::log::LogFatal("lola") << "Invalid ASIL in global/asil-level, terminating.";
            std::terminate();
        case QualityType::kASIL_QM:
            message_rx_queue_size_qm = queue_size;
            break;
        case QualityType::kASIL_B:
            message_rx_queue_size_b = queue_size;
            break;
        // coverity[autosar_cpp14_m6_4_5_violation] std::terminate will terminate this switch clause
        default:
            ::score::mw::log::LogFatal("lola") << "Unknown ASIL parsed from config, terminating";
            std::terminate();
    }
}

void GlobalConfiguration::SetShmSizeCalcMode(const ShmSizeCalculationMode shm_size_calc_mode) noexcept
{
    shm_size_calc_mode_ = shm_size_calc_mode;
}

}  // namespace score::mw::com::impl
