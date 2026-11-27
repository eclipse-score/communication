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

#include "score/someip/routing_module/udp_transport/log.h"
#include "score/someip/routing_module/udp_transport/qnx/transport.h"
#include "score/someip/routing_module/udp_transport/qnx/wakeup_signal.h"
#include "score/someip/routing_module/udp_transport/udp_transport.h"

// qnx kernel headers require a specific include order and cannot be sorted arbitrarily
// clang-format off
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>
#include <qnx/qnx_modload.h>
// clang-format on

static int module_handler(module_t module __unused, int what, void* arg __unused)
{
    switch (what)
    {
        case MOD_LOAD:
        {
            someip_routing_prepare_logging();
            int error = someip_wakeup_init();
            if (error != 0)
            {
                someip_log(SOMEIP_LOG_ERROR, "wakeup initialization failed: %d", error);
                return error;
            }
            error = someip_routing_start();
            if (error != 0)
            {
                someip_log(SOMEIP_LOG_ERROR, "routing module start failed: %d", error);
                return error;
            }
            struct thread* thread = NULL;
            error = kthread_add(someip_qnx_transport_run, NULL, NULL, &thread, 0, 0, "someip");
            if (error != 0)
            {
                someip_log(SOMEIP_LOG_ERROR, "kernel thread creation failed: %d", error);
                someip_routing_stop();
            }
            return error;
        }
        case MOD_QUIESCE:
        case MOD_UNLOAD:
            /* The kernel thread has no coordinated shutdown. */
            return EOPNOTSUPP;
        case MOD_SHUTDOWN:
            return 0;
        default:
            return EOPNOTSUPP;
    }
}

static moduledata_t someip_moduledata = {"someip", module_handler, NULL};

MODULE_VERSION(someip, 1);
DECLARE_MODULE(someip, someip_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);

/* The io-sock loader checks this symbol for API compatibility. */
struct _iosock_module_version iosock_module_version = IOSOCK_MODULE_VER_SYM_INIT;
static int someip_module_version = IOSOCK_VERSION_CUR;
SYSCTL_INT(_qnx_module, OID_AUTO, someip, CTLFLAG_RD, &someip_module_version, 0, "Version");

static void someip_module_uninit(void* arg __unused) {}
/* Linker-set placeholder from the QNX sample; SI_SUB_DUMMY never runs it. */
SYSUNINIT(someip_module_uninit, SI_SUB_DUMMY, SI_ORDER_ANY, someip_module_uninit, NULL);
