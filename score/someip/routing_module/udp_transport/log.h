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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_UDP_TRANSPORT_LOG_H
#define SCORE_SOMEIP_ROUTING_MODULE_UDP_TRANSPORT_LOG_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SOMEIP_LOG_ERROR,
    SOMEIP_LOG_WARNING,
    SOMEIP_LOG_INFO,
    SOMEIP_LOG_DEBUG
} someip_log_severity;

typedef struct
{
    uint64_t last_log_ns;
    uint64_t suppressed;
    int initialized;
} someip_log_limiter;

static inline int someip_log_limiter_at(someip_log_limiter* limiter, uint64_t now_ns, uint64_t* suppressed)
{
    if (limiter->initialized && ((now_ns < limiter->last_log_ns) || ((now_ns - limiter->last_log_ns) < 1000000000ULL)))
    {
        if (limiter->suppressed != UINT64_MAX)
        {
            ++limiter->suppressed;
        }
        return 0;
    }
    *suppressed = limiter->suppressed;
    limiter->suppressed = 0U;
    limiter->last_log_ns = now_ns;
    limiter->initialized = 1;
    return 1;
}

static inline int someip_log_limiter_check(someip_log_limiter* limiter, uint64_t* suppressed)
{
    struct timespec now = {0, 0};
    uint64_t now_ns = limiter->last_log_ns;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)
    {
        now_ns = (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
    }
    return someip_log_limiter_at(limiter, now_ns, suppressed);
}

/* Messages longer than 255 characters are truncated. */
void someip_log(someip_log_severity severity, const char* format, ...) __attribute__((format(printf, 2, 3)));

void someip_routing_log(someip_log_severity severity, const char* message);

/* Thread id of the caller, to attribute CPU time to the data-path thread. */
int someip_thread_id(void);

#ifdef __cplusplus
}
#endif

#endif /* SCORE_SOMEIP_ROUTING_MODULE_UDP_TRANSPORT_LOG_H */
