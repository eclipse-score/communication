score::mw::com - Tutorial
=========================


Introduction
------------


This tutorial will guide you through the basics of using the `score::mw::com` API. Each chapter deals with a specific
feature and chapters build on each other to some degree. Most of the chapters come with specific code snippets/sample
applications and Bazel BUILD files, to see the `score::mw::com` API in action.

If diving into this tutorial and its code samples gives you a deja-vu, that might be because you already know `ara::com`
from the `Adaptive AUTOSAR` standard. The similarities aren't incidental: `score::mw::com` rooted in an implementation
of
the AUTOSAR `ara::com` standard by some of the people, who initially developed the `ara::com` specification. Thus, if
you are already an `ara::com` expert, there is a good chance that you will feel right at home with `score::mw::com`. If
there are bigger deviations - beside the namespace differences - we will note this explicitly.

In some of the chapters we will also shed some light on the design/architecture of `score::mw::com`, but in case you are
really interested in a deeper understanding, please look into the `design documentation <https://github.com/eclipse-score/communication/blob/main/score/mw/com/design/README.md>`__.

Service Orientation, Proxies and Skeletons
------------------------------------------


The communication paradigm of `score::mw::com` follows the paradigm of a so called "service-oriented" architecture. We
spare us a detailed description, because you can easily find a myriad of exhaustive explanations in the web.
Some of the most obvious key characteristics of a service-oriented architecture you will encounter when looking at
`score::mw::com` are:

- separation of the service interface from its implementation(s)
- loose coupling/location transparency between service providers and service consumers
- abstraction of communication details
- discovery mechanisms

Like so many other communication middleware flavors, `score::mw::com` relies on the concept of proxies and skeletons to
implement the service-oriented communication paradigm. A proxy is a client-side (consumer-side) representation of a
service, which provides the same interface as the service itself. A skeleton is a server-side (provider-side)
representation of a service, which implements the service interface and handles incoming requests (method calls) from
clients or delivers data (event/field) updates to the clients.

LoLa binding
------------


Every here and then you might hear the notion `LoLa` in the context of `score::mw::com`. Sometimes it is even used as a
synonym for `score::mw::com`, which is incorrect (being nitpicky here) - LoLa is rather a part (layer) of it:
`score::mw::com` is divided into two layers:

- a layer independent of the concrete technical transport of the data between proxy/skeleton. This layer also contains
  the user-facing public API, which this tutorial wants to teach.
- a technical transport to which the independant layer dispatches to, to realize the data transport. This layer is also
  called `binding`. `score::mw::com` is designed in a way, that this binding/transport can be easily switched. The idea
  is,
  that a user could decide, which binding fits his needs best and then could choose it in a configuration file.

Currently `score:mw::com` has implemented only one binding, which is `LoLa`. This stands for Low Latency and represents
a zero-copy transport via shared-memory. It supports ECU local (within one OS instance) interprocess communication (IPC)
but of course also in-process communication in case provider and consumer happen to live in the same process.
However, if the user needs inter-ECU communication across a network, potentially a different binding capable of
network communication would be needed. As explained above: `score::mw::com` architecture is prepared to add
additional bindings!

.. note::

   Note: Extending `score::mw::com` to support inter-ECU communication can be either solved via introduction of a new
   binding layer capable of network communication as mentioned above or via a gateway approach. This means:
   `score::mw::com`
   always/only uses the `LoLa` binding and a specific central `score::mw::com` application – the gateway app –
   then relays traffic from/to the network.

Tutorial chapters
-----------------


The tutorial is split into chapters. Each chapter has its own directory containing the sample code and a
dedicated `README.md` with the chapter text. Follow them in order – later chapters build on earlier ones.

.. toctree::
   :maxdepth: 1

   Chapter 1: Hello World <chapter_1/README>
   Chapter 2: Hello World configuration refined <chapter_2/README>
   Chapter 3: Finding (multiple) service instances asynchronously <chapter_3/README>
   Chapter 4: Subscription state <chapter_4/README>
   Chapter 5: Sample reception - polling vs. callback <chapter_5/README>
   Chapter 6: Strategies for Sizing sample slots <chapter_6/README>
   Chapter 7: Fields - events on steroids <chapter_7/README>
   Chapter 8: Methods - request/response communication <chapter_8/README>
   Chapter 9: Fields - Get() and Set() <chapter_9/README>
   Chapter 10: Access control for service instances <chapter_10/README>
   Chapter 11: Direct use of an InstanceIdentifier <chapter_11/README>
   Chapter 12: Unit testing <chapter_12/README>
   Chapter 13: Heap-free mw::com for ASIL-B and FFI applications <chapter_13/README>
