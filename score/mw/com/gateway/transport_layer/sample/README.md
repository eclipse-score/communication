<!---- 
*******************************************************************************
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
*******************************************************************************
-->

# Gateway Transport Layer Sample Implementation

This module provides a sample implementation of a mw::com gateway's transport layer based on regular POSIX network sockets
for communication. Since this communication mechanism should be available in most HyperVisor environments, we decided to
add this as a sample implementation. 

## Vendor-specific implementation details

Parts that interact with the underlying shared memory have not been implemented as part of this sample implementation,
as these parts are vendor-specific and depend on the used HyperVisor and its shared memory abilities.
This includes the following methods: 

- `ShmPaths score::mw::com::gateway::ResolveShmPaths(const impl::InstanceSpecifier&)` where a path to access the HV-shared memory region has to be created.
- `ShmSizes score::mw::com::gateway::GetShmSizes(const impl::InstanceSpecifier&)` where the size of the shared memory region has to be determined.
- `void SampleHyperVisorTransport::PreCreateInterVmSharedMemory(const impl::InstanceSpecifier& specifier,
                                                             std::uint32_t shm_control_size,
                                                             std::uint32_t shm_data_size)` where the shared memory region needs to be opened
