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
#ifndef SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_LOG_H
#define SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_LOG_H

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

/* Logs to slog with the prefix "[someip]"; messages longer than 255 characters are truncated. */
void someip_log(someip_log_severity severity, const char* format, ...) __attribute__((format(printf, 2, 3)));

/* Thread id of the caller, to attribute CPU time to the module's kernel thread. */
int someip_thread_id(void);

#ifdef __cplusplus
}
#endif

#endif /* SCORE_SOMEIP_ROUTING_MODULE_IOSOCK_TRANSPORT_LOG_H */
