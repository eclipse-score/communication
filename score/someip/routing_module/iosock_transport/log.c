/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/someip/routing_module/iosock_transport/log.h"

#include <pthread.h>
#include <stdarg.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>
#include <stdio.h>

/* A fixed stack buffer avoids heap allocation on the data path. */
#define SOMEIP_LOG_MESSAGE_SIZE 256U

static int to_slog_severity(someip_log_severity severity)
{
    switch (severity)
    {
        case SOMEIP_LOG_ERROR:
            return _SLOG_ERROR;
        case SOMEIP_LOG_WARNING:
            return _SLOG_WARNING;
        case SOMEIP_LOG_INFO:
            return _SLOG_INFO;
        case SOMEIP_LOG_DEBUG:
        default:
            return _SLOG_DEBUG1;
    }
}

void someip_log(someip_log_severity severity, const char* format, ...)
{
    char message[SOMEIP_LOG_MESSAGE_SIZE];
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    (void)slogf(_SLOGC_NETWORK, to_slog_severity(severity), "[someip] %s", message);
}

int someip_thread_id(void)
{
    return (int)pthread_self();
}
