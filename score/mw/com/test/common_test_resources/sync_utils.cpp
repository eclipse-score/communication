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

#include "score/mw/com/test/common_test_resources/sync_utils.h"

#include <chrono>
#include <fstream>
#include <iostream>

bmw::mw::com::test::SyncCoordinator::SyncCoordinator(amp::string_view file_name) : file_name_(std::move(file_name)) {}

void bmw::mw::com::test::SyncCoordinator::Signal() noexcept
{
    std::ofstream outfile(file_name_.data());
    outfile << "Synchronization Signal is sent.\n"
            << std::endl;
    std::cout << "File " << file_name_.data() << " is created, Synchronize Signal is sent.\n";
    outfile.close();
}

void bmw::mw::com::test::SyncCoordinator::CleanUp(amp::string_view file_name) noexcept
{
    std::cout << "Cleanup, deleting the synchronization file." << std::endl;
    auto result = std::remove(file_name.data());
    if (result != 0)
    {
        std::cerr << "File deletion failed\n";
    }
    else
    {
        std::cout << "File deleted successfully\n";
    }
}

void bmw::mw::com::test::SyncCoordinator::CheckFileCreation(
    std::shared_ptr<bmw::concurrency::InterruptiblePromise<void>> promise,
    const amp::stop_token &stop_token) noexcept
{
    std::ifstream file;
    while (!stop_token.stop_requested())
    {
        file.open(file_name_.data());
        if (file)
        {
            std::cout << "File exists, succeeded to synchronize" << std::endl;
            file.close();
            promise->SetValue();
            return;
        }
        std::cout << "File doesn't exist yet, failed to synchronize" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }
}

bmw::concurrency::InterruptibleFuture<void> bmw::mw::com::test::SyncCoordinator::Wait(
    const amp::stop_token &stop_token) noexcept
{
    auto promise = std::make_shared<bmw::concurrency::InterruptiblePromise<void>>();
    bmw::concurrency::InterruptibleFuture<void> future = promise->GetInterruptibleFuture().value();

    checkfile_thread_ = amp::jthread(
        [this, promise = std::move(promise), stop_token]() noexcept
        { CheckFileCreation(promise, stop_token); });
    return future;
}
