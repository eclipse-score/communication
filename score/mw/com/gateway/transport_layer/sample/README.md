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

The functionality a `Gateway Transport Layer` has to provide can be roughly divided into two parts:

1. Provide a message transport between the gateways, so that the gateways instances can interact with specific control 
messages.
2. Provide the data exchange for the service-instance related data. In case of a non-copying shared memory setup, 
this means to make shared-memory accessible between VMs. In case of a copying `Transport Layer` this means setting up DMA 
data transfers and the like.


For the first part this module provides a sample implementation of a mw::com gateway's transport layer based on regular 
POSIX network sockets for communication. Since this communication mechanism should be available in most HyperVisor 
environments, we decided to add this as a sample implementation. 

## Vendor-specific implementation details

Our sample Transport Layer implementation returns `true` for an API call of `IsMemorySharingSupported`. So it is a sample for a `Gateway Transport Layer` 
for an inter-VM setup, where memory sharing between VMs is possible.
It can not fully implement the second part (see above) with regard to making shared-memory objects visible/accessible 
between gateway domains (VMs) as this is vendor/hypervisor/OS specific. Thus, there are TODOs in the code.

However, the basic assumption in this sample implementation is:

- in the `ProvideService` request, the destination Transport Layer receives from the source side, there is an 
`InstanceSpecifier` and the sizes of the control/data shm-objects.
- from the `InstanceSpecifier` the Transport Layer can deduce the identification of the shared-memory objects created on the source side.
- it can verify, whether the shm-object sizes communicated in the request match with the shm-objects it opened.
