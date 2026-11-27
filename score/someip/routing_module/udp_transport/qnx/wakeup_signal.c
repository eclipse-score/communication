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
#include "score/someip/routing_module/udp_transport/qnx/wakeup_signal.h"

#include "score/someip/routing_module/udp_transport/udp_transport.h"

#include <semaphore.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>

#define MILLISECONDS_PER_SECOND 1000U
#define NANOSECONDS_PER_MILLISECOND 1000000L
#define NANOSECONDS_PER_SECOND 1000000000L

static sem_t semaphore;
/* At most one outstanding sem_post: wakeups while the kernel thread is awake coalesce. */
static atomic_bool posted;

int someip_wakeup_init(void)
{
    return (sem_init(&semaphore, 0, 0U) == 0) ? 0 : errno;
}

void someip_transport_wakeup(void)
{
    if (!atomic_exchange_explicit(&posted, true, memory_order_release))
    {
        (void)sem_post(&semaphore);
    }
}

void someip_wakeup_wait(unsigned int timeout_ms)
{
    struct timespec deadline;
    (void)clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += (time_t)(timeout_ms / MILLISECONDS_PER_SECOND);
    deadline.tv_nsec += (long)(timeout_ms % MILLISECONDS_PER_SECOND) * NANOSECONDS_PER_MILLISECOND;
    if (deadline.tv_nsec >= NANOSECONDS_PER_SECOND)
    {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= NANOSECONDS_PER_SECOND;
    }
    if (sem_timedwait_monotonic(&semaphore, &deadline) == 0)
    {
        /* Acquire before the next poll and drain, so a racing wakeup is either handled by them or posts again. */
        (void)atomic_exchange_explicit(&posted, false, memory_order_acquire);
    }
}
