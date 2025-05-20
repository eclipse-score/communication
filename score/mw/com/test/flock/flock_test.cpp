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

#include "score/filesystem/filesystem.h"
#include "score/memory/shared/flock/exclusive_flock_mutex.h"
#include "score/memory/shared/flock/shared_flock_mutex.h"
#include "score/memory/shared/lock_file.h"

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#if defined(__QNXNTO__)
const std::string kBaseFolder{"/tmp_discovery"};
#else
const std::string kBaseFolder{"/tmp"};
#endif
const std::string kTestDirName{"flockTest"};
const std::string kTestDir{kBaseFolder + "/" + kTestDirName};
const std::string kSharedLockFileName{"shared_locked"};
const std::string kExclusiveLockFileName{"exclusive_locked"};

namespace bmw
{
    namespace mw
    {
        namespace com
        {
            namespace test
            {

                namespace
                {
                    const char kChildDone = 'Z';

                    /// \brief Action the forked child process does.
                    /// \details Child process creates TWO "lock-files" under /tmp_discovery/flockTest. One it flocks with a shared-lock,
                    ///          the other it locks with an exclusive-lock. After it has created and flocked those files it notifies its
                    ///          parent via writing a character to the given fd (fd_to_write_to). Then it waits to get killed by the parent.
                    /// \param fd_to_write_to
                    void DoChildActions(int fd_to_write_to)
                    {
                        using namespace bmw::memory::shared;

                        FILE *stream;
                        stream = fdopen(fd_to_write_to, "w");
                        if (stream == nullptr)
                        {
                            std::cerr << "Child: Can't open pipe fd!" << std::endl;
                            return;
                        }

                        bmw::filesystem::StandardFilesystem fs;
                        const auto create_dir = fs.CreateDirectory(kTestDir);
                        if (!create_dir.has_value())
                        {
                            std::cerr << __FILE__ << ":" << __LINE__ << ":Create directory " << kTestDir << "failed" << create_dir.error()
                                      << std::endl;
                            return;
                        }
                        std::cout << "Created directory:" << kTestDir << std::endl;

                        // Create shared-lock file and exclusive-lock file
                        auto shared_lock_file = LockFile::Create(kTestDir + "/" + kSharedLockFileName);
                        if (!shared_lock_file.has_value())
                        {
                            std::cerr << "Child: Can't create shared-lock-file" << std::endl;
                            return;
                        }
                        auto exclusive_lock_file = LockFile::Create(kTestDir + "/" + kExclusiveLockFileName);
                        if (!exclusive_lock_file.has_value())
                        {
                            std::cerr << "Child: Can't create exclusive-lock-file" << std::endl;
                            return;
                        }

                        // now lock them with a shared resp. exclusive lock ...
                        auto shared_flock_mutex = SharedFlockMutex(shared_lock_file.value());
                        shared_flock_mutex.lock();
                        std::cout << "Child: Locked shared-lock-file" << std::endl;

                        auto exclusive_flock_mutex = ExclusiveFlockMutex(exclusive_lock_file.value());
                        exclusive_flock_mutex.lock();
                        std::cout << "Child: Locked exclusive-lock-file" << std::endl;

                        // send message to parent, that we are done ...
                        if (fputc(kChildDone, stream) == EOF)
                        {
                            std::cerr << "Child: Failed to inform parent/controller, that I'm done!" << std::endl;
                            return;
                        }
                        else
                        {
                            std::cout << "Child: Informed parent/controller, that I'm done!" << std::endl;
                        }
                        fflush(stream);

                        // sleep until we get killed
                        while (true)
                        {
                            std::cout << "Child: Going to sleep" << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds{5U});
                            std::cout << "Child: Wake up again." << std::endl;
                        }
                    }

                    /// \brief Called by the parent process to check the file, which has been flocked via a shared-lock by the child.
                    /// \details It checks, that another shared-lock can be placed via flock an a file, which already has a shared-lock.
                    ///          It checks, that NO exclusive-lock can be placed via flock an a file, which already has a shared-lock.
                    /// \return EXIT_SUCCESS, if the checks are ok, EXIT_FAILURE otherwise.
                    int CheckSharedLockedFile()
                    {
                        using namespace bmw::memory::shared;

                        auto shared_lock_file = LockFile::Open(kTestDir + "/" + kSharedLockFileName);
                        if (!shared_lock_file.has_value())
                        {
                            std::cerr << "Controller: Can't open shared-lock-file" << std::endl;
                            return EXIT_FAILURE;
                        }
                        auto shared_flock_mutex = SharedFlockMutex(shared_lock_file.value());
                        auto lock_success = shared_flock_mutex.try_lock();
                        if (!lock_success)
                        {
                            std::cerr << "Controller: Failed to place shared-lock on shared-lock-file" << std::endl;
                            return EXIT_FAILURE;
                        }

                        auto exclusive_flock_mutex = ExclusiveFlockMutex(shared_lock_file.value());
                        lock_success = exclusive_flock_mutex.try_lock();
                        if (lock_success)
                        {
                            std::cerr << "Controller: Error - was able to place exclusive lock on already shared-locked shared-lock-file"
                                      << std::endl;
                            return EXIT_FAILURE;
                        }
                        else
                        {
                            std::cout << "Controller: SUCCESS - could NOT exclusively lock an already shared-locked file!" << std::endl;
                        }
                        return EXIT_SUCCESS;
                    }

                    /// \brief Called by the parent process to check the file, which has been flocked via an exclusive-lock by the child.
                    /// \details It checks, that NO shared-lock can be placed via flock an a file, which already has an exclusive-lock.
                    ///          It checks, that NO exclusive-lock can be placed via flock an a file, which already has a shared-lock.
                    /// \return EXIT_SUCCESS, if the checks are ok, EXIT_FAILURE otherwise.
                    int CheckExclusiveLockedFile()
                    {
                        using namespace bmw::memory::shared;

                        auto exclusive_lock_file = LockFile::Open(kTestDir + "/" + kExclusiveLockFileName);
                        if (!exclusive_lock_file.has_value())
                        {
                            std::cerr << "Controller: Can't open exclusive-lock-file" << std::endl;
                            return EXIT_FAILURE;
                        }
                        auto shared_flock_mutex = SharedFlockMutex(exclusive_lock_file.value());
                        auto lock_success = shared_flock_mutex.try_lock();
                        if (lock_success)
                        {
                            std::cerr << "Controller: Error - was able to successfully placed shared-lock on exclusive-lock-file"
                                      << std::endl;
                            return EXIT_FAILURE;
                        }

                        auto exclusive_flock_mutex = ExclusiveFlockMutex(exclusive_lock_file.value());
                        lock_success = exclusive_flock_mutex.try_lock();
                        if (lock_success)
                        {
                            std::cerr
                                << "Controller: Error - was able to place exclusive-lock on already exclusively-locked exclusive-lock-file"
                                << std::endl;
                            return EXIT_FAILURE;
                        }

                        return EXIT_SUCCESS;
                    }

                    /// \brief Called by the parent process to check both files, which have been previously flocked by the child.
                    ///        This is called after the child has died!
                    /// \details It checks, that an exclusive-lock can be placed via flock on both files.
                    /// \return EXIT_SUCCESS, if the checks are ok, EXIT_FAILURE otherwise.
                    int LockBothFilesExclusively()
                    {
                        using namespace bmw::memory::shared;

                        auto exclusive_lock_file = LockFile::Open(kTestDir + "/" + kExclusiveLockFileName);
                        if (!exclusive_lock_file.has_value())
                        {
                            std::cerr << "Controller: Can't open exclusive-lock-file" << std::endl;
                            return EXIT_FAILURE;
                        }

                        auto exclusive_flock_mutex = ExclusiveFlockMutex(exclusive_lock_file.value());
                        auto lock_success = exclusive_flock_mutex.try_lock();
                        if (!lock_success)
                        {
                            std::cerr << "Controller: Error - couldn't exclusively-lock exclusive-lock-file" << std::endl;
                            return EXIT_FAILURE;
                        }

                        auto shared_lock_file = LockFile::Open(kTestDir + "/" + kSharedLockFileName);
                        if (!shared_lock_file.has_value())
                        {
                            std::cerr << "Controller: Can't open shared-lock-file" << std::endl;
                            return EXIT_FAILURE;
                        }

                        exclusive_flock_mutex = ExclusiveFlockMutex(shared_lock_file.value());
                        lock_success = exclusive_flock_mutex.try_lock();
                        if (!lock_success)
                        {
                            std::cerr << "Controller: Error - couldn't exclusively-lock shared-lock-file" << std::endl;
                            return EXIT_FAILURE;
                        }

                        return EXIT_SUCCESS;
                    }

                    /// \brief Function called by parent/controller to wait for child notification about created/flocked files.
                    /// \param fd_to_read_from fd to read from to get child notification (reading pipe end)
                    /// \return true in case child notified in time, false else
                    bool WaitForChildFinished(int fd_to_read_from)
                    {
                        // we set our pipe/fd to non-blocking.
                        auto fcntl_result = fcntl(fd_to_read_from, F_SETFL, fcntl(fd_to_read_from, F_GETFL) | O_NONBLOCK);
                        if (fcntl_result == -1)
                        {
                            std::cerr << "Controller: Error changing pipe to O_NONBLOCK: " << strerror(errno) << ", terminating."
                                      << std::endl;
                            return false;
                        }
                        bool child_is_done{false};
                        FILE *stream;
                        int c;
                        stream = fdopen(fd_to_read_from, "r");
                        if (stream == nullptr)
                        {
                            std::cerr << "Controller: Can't open pipe fd!" << std::endl;
                            return false;
                        }
                        // Wait until child has created flocked files ...
                        for (auto i = 0; i < 10; ++i)
                        {
                            c = fgetc(stream);
                            if (c == EOF)
                            {
                                if (errno == EAGAIN)
                                {
                                    // child hasn't sent notification, that it is done yet
                                    std::cerr << "Controller: Child has not yet created flock-files. Go to sleep again and recheck later."
                                              << std::endl;
                                    std::this_thread::sleep_for(std::chrono::milliseconds{100U});
                                }
                                else
                                {
                                    std::cerr << "Controller: Error reading from pipe: " << strerror(errno) << ", terminating."
                                              << std::endl;
                                    return false;
                                }
                            }
                            else if (c == bmw::mw::com::test::kChildDone)
                            {
                                child_is_done = true;
                                break;
                            }
                            else
                            {
                                std::cerr << "Controller: Error reading from pipe. Unexpected char: " << c << ", terminating." << std::endl;
                                return false;
                            }
                        }
                        return child_is_done;
                    }

                } // namespace

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw

int main()
{
    // We use a simple pipe to communicate with a child, we will fork.
    int client_pipe_fds[2];
    if (pipe(client_pipe_fds) != 0)
    {
        std::cerr << "Controller: Error creating pipe: " << strerror(errno) << ", terminating.";
        return EXIT_FAILURE;
    }

    // fork a child, which will create and flock files, which we then will try to also flock from parent side ...
    const pid_t fork_child_pid = fork();
    switch (fork_child_pid)
    {
    case -1:
        std::cerr << "Error forking child process: " << strerror(errno) << ", terminating." << std::endl;
        return EXIT_FAILURE;

    case 0:
        // this is the case of the child process
        close(client_pipe_fds[0]);
        bmw::mw::com::test::DoChildActions(client_pipe_fds[1]);
        return EXIT_SUCCESS;

    default:
        std::cout << "Controller: Child process forked successfully." << std::endl;
        auto child_is_done = bmw::mw::com::test::WaitForChildFinished(client_pipe_fds[0]);
        if (!child_is_done)
        {
            std::cerr << "Controller: Didn't get child notification in time, terminating." << std::endl;
            return EXIT_FAILURE;
        }
        // Now do our checks/tests on the files flocked by the child.
        auto result = bmw::mw::com::test::CheckSharedLockedFile();
        if (result == EXIT_FAILURE)
        {
            std::cout << "Controller: Shared-Locking test failed!" << std::endl;
            return EXIT_FAILURE;
        }

        result = bmw::mw::com::test::CheckExclusiveLockedFile();
        if (result == EXIT_FAILURE)
        {
            std::cout << "Controller: Exclusive-Locking test failed!" << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "Controller: Killing child process" << std::endl;
        const auto kill_result = kill(fork_child_pid, SIGKILL);
        if (kill_result != 0)
        {
            std::cerr << "Controller: Error killing child process: " << strerror(errno) << ".";
            return EXIT_FAILURE;
        }

        // sleep some time and check the lock-files after child is dead
        std::this_thread::sleep_for(std::chrono::seconds{1U});
        // now both files should be able to be locked exclusively
        result = bmw::mw::com::test::LockBothFilesExclusively();
        if (result == EXIT_FAILURE)
        {
            std::cout << "Controller: Exclusive-Locking test after child died failed!" << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "Controller: SUCCESS! All flock tests succeeded!" << std::endl;
        return EXIT_SUCCESS;
    }
}
