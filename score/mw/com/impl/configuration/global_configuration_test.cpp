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

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

static constexpr auto kDefaultQualityType = QualityType::kASIL_QM;
static constexpr std::int32_t kDefaultMinNumMessagesRxQueue{10};
static constexpr std::int32_t kDefaultMinNumMessagesTxQueue{20};
static constexpr auto kDefaultShmSizeCalculationMode = ShmSizeCalculationMode::kSimulation;

TEST(GlobalConfigurationTest, GettingProcessAsilLevelBeforeSetValueReturnsDefault)
{
    GlobalConfiguration global_configuration{};
    const auto get_quality_type = global_configuration.GetProcessAsilLevel();
    EXPECT_EQ(get_quality_type, kDefaultQualityType);
}

TEST(GlobalConfigurationTest, GettingProcessAsilLevelReturnsSetValue)
{
    GlobalConfiguration global_configuration{};
    {
        const auto set_quality_type = QualityType::kASIL_QM;
        global_configuration.SetProcessAsilLevel(set_quality_type);
        const auto get_quality_type = global_configuration.GetProcessAsilLevel();
        EXPECT_EQ(get_quality_type, set_quality_type);
    }
    {
        const auto set_quality_type = QualityType::kASIL_B;
        global_configuration.SetProcessAsilLevel(set_quality_type);
        const auto get_quality_type = global_configuration.GetProcessAsilLevel();
        EXPECT_EQ(get_quality_type, set_quality_type);
    }
}

TEST(GlobalConfigurationTest, GettingReceiverMessageQueueSizeBeforeSetValueReturnsDefault)
{
    GlobalConfiguration global_configuration{};
    {
        const auto set_quality_type = QualityType::kASIL_QM;
        const std::int32_t set_message_queue_size{100};
        global_configuration.SetReceiverMessageQueueSize(set_quality_type, set_message_queue_size);
        const auto get_message_queue_size = global_configuration.GetReceiverMessageQueueSize(set_quality_type);
        EXPECT_EQ(get_message_queue_size, set_message_queue_size);
    }
    {
        const auto set_quality_type = QualityType::kASIL_B;
        const std::int32_t set_message_queue_size{100};
        global_configuration.SetReceiverMessageQueueSize(set_quality_type, set_message_queue_size);
        const auto get_message_queue_size = global_configuration.GetReceiverMessageQueueSize(set_quality_type);
        EXPECT_EQ(get_message_queue_size, set_message_queue_size);
    }
}

TEST(GlobalConfigurationTest, GettingReceiverMessageQueueSizeReturnsSetValue)
{
    GlobalConfiguration global_configuration{};
    {
        const auto get_quality_type = QualityType::kASIL_QM;
        const auto get_message_queue_size = global_configuration.GetReceiverMessageQueueSize(get_quality_type);
        EXPECT_EQ(get_message_queue_size, kDefaultMinNumMessagesRxQueue);
    }
    {
        const auto get_quality_type = QualityType::kASIL_QM;
        const auto get_message_queue_size = global_configuration.GetReceiverMessageQueueSize(get_quality_type);
        EXPECT_EQ(get_message_queue_size, kDefaultMinNumMessagesRxQueue);
    }
}

TEST(GlobalConfigurationTest, GettingSenderMessageQueueSizeReturnsSetValue)
{
    GlobalConfiguration global_configuration{};

    const std::int32_t set_message_queue_size{100};
    global_configuration.SetSenderMessageQueueSize(set_message_queue_size);
    const auto get_message_queue_size = global_configuration.GetSenderMessageQueueSize();
    EXPECT_EQ(get_message_queue_size, set_message_queue_size);
}

TEST(GlobalConfigurationTest, GettingSenderMessageQueueSizeBeforeSetValueReturnsDefault)
{
    GlobalConfiguration global_configuration{};

    const auto get_message_queue_size = global_configuration.GetSenderMessageQueueSize();
    EXPECT_EQ(get_message_queue_size, kDefaultMinNumMessagesTxQueue);
}

TEST(GlobalConfigurationTest, GettingShmSizeCalcModeReturnsSetValue)
{
    GlobalConfiguration global_configuration{};

    {
        const auto set_shm_calc_size_mod{ShmSizeCalculationMode::kEstimation};
        global_configuration.SetShmSizeCalcMode(set_shm_calc_size_mod);
        const auto get_shm_calc_size_mod = global_configuration.GetShmSizeCalcMode();
        EXPECT_EQ(get_shm_calc_size_mod, set_shm_calc_size_mod);
    }
    {
        const auto set_shm_calc_size_mod{ShmSizeCalculationMode::kSimulation};
        global_configuration.SetShmSizeCalcMode(set_shm_calc_size_mod);
        const auto get_shm_calc_size_mod = global_configuration.GetShmSizeCalcMode();
        EXPECT_EQ(get_shm_calc_size_mod, set_shm_calc_size_mod);
    }
}

TEST(GlobalConfigurationTest, GettingShmSizeCalcModeBeforeSetValueReturnsDefault)
{
    GlobalConfiguration global_configuration{};

    const auto get_shm_calc_size_mod = global_configuration.GetShmSizeCalcMode();
    EXPECT_EQ(get_shm_calc_size_mod, kDefaultShmSizeCalculationMode);
}

TEST(GlobalConfigurationDeathTest, GetReceiverMessageQueueSize_InvalidQualityType)
{
    // Given a default constructed GlobalConfiguration
    GlobalConfiguration global_configuration{};

    // expect, that it terminates/dies, when calling GetReceiverMessageQueueSize with an invalid QualityType
    EXPECT_DEATH(global_configuration.GetReceiverMessageQueueSize(QualityType::kInvalid), ".*");
}

TEST(GlobalConfigurationDeathTest, GetReceiverMessageQueueSize_UnknownQualityType)
{
    // Given a default constructed GlobalConfiguration
    GlobalConfiguration global_configuration{};

    // expect, that it terminates/dies, when calling GetMessageQueueSize with an unknown QualityType
    EXPECT_DEATH(global_configuration.GetReceiverMessageQueueSize(static_cast<QualityType>(99)), ".*");
}

TEST(GlobalConfigurationDeathTest, SetReceiverMessageQueueSize_InvalidQualityType)
{
    // Given a default constructed GlobalConfiguration
    GlobalConfiguration global_configuration{};

    // expect, that it terminates/dies, when calling SetReceiverMessageQueueSize with an invalid QualityType
    EXPECT_DEATH(global_configuration.SetReceiverMessageQueueSize(QualityType::kInvalid, 42), ".*");
}

TEST(GlobalConfigurationDeathTest, SetReceiverMessageQueueSize_UnknownQualityType)
{
    // Given a default constructed GlobalConfiguration
    GlobalConfiguration global_configuration{};

    // expect, that it terminates/dies, when calling SetReceiverMessageQueueSize with an unknown QualityType
    EXPECT_DEATH(global_configuration.SetReceiverMessageQueueSize(static_cast<QualityType>(99), 42), ".*");
}

}  // namespace
}  // namespace score::mw::com::impl
