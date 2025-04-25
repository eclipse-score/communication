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
#include "score/memory/shared/memory_resource_registry.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/shared_memory_resource.h"

#include <boost/program_options.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

template <typename T>
using shm_vector = std::vector<T, score::memory::shared::PolymorphicOffsetPtrAllocator<T>>;

/// \brief Layout of the memory within our shared-memory objects used in the test.
struct MemoryLayout
{
    MemoryLayout(const score::memory::shared::MemoryResourceProxy* resource) : data{resource} {}

    shm_vector<std::uint32_t> data;
};

/// \brief The size to which the vector (data) within the shared-memory will get expanded and then iterated over.
constexpr std::size_t kTestVectorSize{1000};

void handle_eptr(std::exception_ptr eptr)  // passing by value is ok
{
    try
    {
        if (eptr)
        {
            std::rethrow_exception(eptr);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Caught exception \"" << e.what() << "\"\n";
    }
}

/// \brief activity each spawned thread does.
/// \detail thread does kTestVectorSize accesses to data member in shared memory object/ManagedMemoryResource.
///         It randomly chooses thereby for each access in which of the numMemResUsed shared memory objects it does
///         the access.
/// \param shmem_sections vector of initially created shared memory object/ManagedMemoryResource and corresponding
///        pointers to the created data structure (MemoryLayout) created within it.
/// \param numMemResUsed number of shared memory object/ManagedMemoryResource to be used (needs to be <=
/// shmem_sections.size()
void ThreadAction(
    std::vector<std::pair<std::shared_ptr<score::memory::shared::ManagedMemoryResource>, MemoryLayout*>>& shmem_sections,
    std::size_t numMemResUsed)
{
    try
    {
        for (std::size_t counter = 0; counter < kTestVectorSize; counter++)
        {
            int index = std::rand() % numMemResUsed;
            // Accessing/writing element of vector in shared-mem will lead to OffsetPtr operator[]() call and
            // eventually a bounds check!
            shmem_sections[index].second->data[counter] = 42u;
        }
    }
    catch (...)
    {
        std::exception_ptr eptr;
        eptr = std::current_exception();
        handle_eptr(eptr);
    }
}

}  // namespace

/**
 * \brief   Small test program to test the efficiency/overhead of OffsetPtr bounds-checking.
 *
 * \details Test program creates a configurable (param num-shared-mem-total) number of shared-memory-objects each with a
 * size of 64KB. Then it spawns a configurable (param num-threads) number of threads, where each thread iterates over
 * the data-vector placed in each shared-memory-object (see struct MemoryLayout), whereby de-referencing implicitly
 * OffsetPtrs, which leads to bounds-checking.
 * Each thread only accesses a configurable number (param num-shared-mem-used) of shared-memory-objects. This number is
 * typically smaller than the number of total number of shared-memory-objects for the entire process.
 * With this distinction we simulate a more realistic setup, where threads within a LoLa application don't interact with
 * all shared-memory-objects for all proxies/skeletons of the process, but a subset! The threads randomly choose in each
 * vector access, which shared-memory-object to use.
 *
 * With the possibility to have num-shared-mem-total > num-shared-mem-used, one can simulate a MemoryResourceRegistry
 * with lots of entries, which makes the lookup-speed for memory-regions done within bounds-checking a bit slower!
 *
 * With the possibility to increase the num-shared-mem-used, the threads will access more different
 * shared-memory-objects, which can decrease the hit rate of our thread local region-caches.
 *
 * With param enable-bounds-check one can completely turn off the bounds-checking. This allows running the same test
 * setup once WITH and WITHOUT bounds-checking and get insight about the performance penalty, which bounds-checking
 * adds.
 *
 * \return always EXIT_SUCCESS
 */
int main(int argc, char* argv[])
{
    namespace po = boost::program_options;

    std::size_t number_registered_mem_res_total;
    std::size_t number_used_mem_res;
    std::size_t number_concurrent_threads;
    bool enable_bounds_check;

    po::options_description options;
    // clang-format off
    options.add_options()
        ("help", "Display the help message")
        ("num-shared-mem-total", po::value<std::size_t>(&number_registered_mem_res_total)->default_value(10U), "Total number of mapped shared-memory objects")
        ("num-shared-mem-used", po::value<std::size_t>(&number_used_mem_res)->default_value(2U), "Used number of mapped shared-memory objects")
        ("num-threads", po::value<std::size_t>(&number_concurrent_threads)->default_value(4), "Number of concurrent threads accessing OffsetPtrs")
        ("enable-bounds-check", po::value<bool>(&enable_bounds_check)->default_value(true), "Enable bounds-checking");
    // clang-format on
    po::variables_map args;
    const auto parsed_args =
        po::command_line_parser{argc, argv}
            .options(options)
            .style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
            .run();
    po::store(parsed_args, args);

    if (args.count("help") > 0U)
    {
        std::cerr << options << std::endl;
        return EXIT_FAILURE;
    }

    po::notify(args);

    std::cerr << "Running OffsetPtr Benchmark with:\nnum-shared-mem-total: " << number_registered_mem_res_total
              << "\nnum-shared-mem-used: " << number_used_mem_res << "\nnum-threads: " << number_concurrent_threads
              << "\nNum. vector elements accessed per thread: " << kTestVectorSize
              << "\nenable-bounds-check: " << enable_bounds_check << "\n";

    score::memory::shared::EnableOffsetPtrBoundsChecking(enable_bounds_check);
    std::vector<std::pair<std::shared_ptr<score::memory::shared::ManagedMemoryResource>, MemoryLayout*>> shmem_sections;
    MemoryLayout* ptr_to_sec{nullptr};
    auto callback = [&ptr_to_sec](std::shared_ptr<score::memory::shared::SharedMemoryResource> resource) {
        ptr_to_sec = resource->construct<MemoryLayout>(resource->getMemoryResourceProxy());
    };

    std::vector<std::string> created_shm_object_paths{};
    for (uint32_t i = 0; i < number_registered_mem_res_total; i++)
    {
        std::string path = "/testing_shared_memory_" + std::to_string(i);
        auto resource = score::memory::shared::SharedMemoryFactory::Create(path, callback, 65536);
        created_shm_object_paths.push_back(path);
        shmem_sections.push_back({resource, ptr_to_sec});
        for (std::size_t counter = 0; counter < kTestVectorSize; counter++)
        {
            shmem_sections[i].second->data.push_back(counter);
        }
    }

    std::cerr << "Created " << number_registered_mem_res_total << " SharedMemoryResources\n";

    // now shuffle our vector, so that later it is "random", which shm_sections we chose to access.
    auto rng = std::default_random_engine{};
    std::shuffle(shmem_sections.begin(), shmem_sections.end(), rng);

    std::vector<std::thread> test_threads;

    auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < number_concurrent_threads; i++)
    {
        std::thread th(ThreadAction, std::ref(shmem_sections), number_used_mem_res);
        test_threads.push_back(std::move(th));
    }

    for (auto& thread : test_threads)
    {
        thread.join();
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> runtime = end - start;

    std::cerr << "Runtime: " << runtime.count() << std::endl;

    for (const auto& path : created_shm_object_paths)
    {
        score::memory::shared::SharedMemoryFactory::Remove(path);
    }

    return EXIT_SUCCESS;
}
