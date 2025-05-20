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

#include "score/mw/com/test/common_test_resources/assert_handler.h"

// #include <amp_assert.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>

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

                    /*void assert_handler(const amp::handler_parameters &params)
                    {
                        std::cerr << "Assertion \"" << params.condition << "\" failed";
                        if (params.message != nullptr)
                        {
                            std::cerr << ": " << params.message;
                        }
                        std::cerr << " (" << params.file << ':' << params.line << ")" << std::endl;
                        std::cerr.flush();
                        const char *const no_abort = std::getenv("ASSERT_NO_CORE");
                        if (no_abort != nullptr)
                        {
                            std::cerr << "Would not coredump on \"" << no_abort << "\"" << std::endl;
                            if (std::strcmp(no_abort, params.condition) == 0)
                            {
                                std::cerr << "... matched." << std::endl;
                                std::cerr.flush();
                                std::quick_exit(1);
                            }
                            std::cerr << "... not matched." << std::endl;
                        }
                        std::cerr.flush();
                    }*/

                } // namespace

                void SetupAssertHandler()
                {
                    // disabled: amp::set_assertion_handler(assert_handler);
                }

            } // namespace test
        } // namespace com
    } // namespace mw
} // namespace bmw
