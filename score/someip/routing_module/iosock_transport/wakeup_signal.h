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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_WAKEUP_SIGNAL_H
#define SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_WAKEUP_SIGNAL_H

/* Kernel thread side of someip_iosock_wakeup(); a POSIX semaphore is used because taskqueue_enqueue() and wakeup()
 * hang io-sock when called from non-kernel threads. */

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 0 or an errno. */
int someip_wakeup_init(void);

/* Returns after a wakeup or when timeout_ms elapsed. */
void someip_wakeup_wait(unsigned int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_WAKEUP_SIGNAL_H */
