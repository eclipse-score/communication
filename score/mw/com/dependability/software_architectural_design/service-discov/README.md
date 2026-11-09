<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Service Discovery

The service discovery in mw::com is defined as a dynamic service discovery at runtime.

## High Level Architecture

In the following chapter the service discovery fot methods is described.

### Methods

For Methods the service discovery consists of two parts:

- Skeleton offering a service
- Proxy using a service

#### Skeleton

![Offer Service](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/eclipse-score/communication/master/score/mw/com/dependability/software_architectural_design/service-discov/assets/offer_service.puml)

#### Proxy

![Find Service](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/eclipse-score/communication/master/score/mw/com/dependability/software_architectural_design/service-discov/assets/find_service.puml)


![Create Proxy Instance](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/eclipse-score/communication/master/score/mw/com/dependability/software_architectural_design/service-discov/assets/create_proxy_instance.puml)
