Chapter 1: Hello World
======================


Let's directly jump into our first chapter, which implements – as expected – a simple "Hello World"
application:
The provided service contains just a single event named "message", which is of a fixed string type. The provider of the
service instance updates this event cyclically with a "Hello World" message plus a counter. The consumer of the service
instance subscribes to the event and checks cyclically for updates. Whenever there is an update, the consumer prints the
new message to the console.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_1`. It contains the following
files:

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/BUILD>`__
     - This file contains bazel targets for this example.
   * - `consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/consumer.cpp>`__
     - Implementation of the service consumer app. The `main()` for the consumer
   * - `consumer.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/consumer.h>`__
     - Header (empty - we just aways want to have cpp/h pairs) of the service consumer.
   * - `hello_world_service.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/hello_world_service.cpp>`__
     - This file is empty as the service interface is completely defined in the header.
   * - `hello_world_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/hello_world_service.h>`__
     - This file contains the definition of the service interface.
   * - `logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for our apps.
   * - `provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/provider.cpp>`__
     - Implementation of the service provider.
   * - `provider.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/provider.h>`__
     - Header (empty) of the service provider.


----------------------------------------

How to run the example
~~~~~~~~~~~~~~~~~~~~~~


To run the example (on host/Linux), you can use the following bazel commands:

.. code-block:: bash

   # Build the provider and consumer targets
   bazel build //score/mw/com/doc/tutorial/chapter_1:provider-tar
   bazel build //score/mw/com/doc/tutorial/chapter_1:consumer-tar


The bazel build above creates tar-archives for the provider application and the consumer application.
They should then be located under `bazel-bin/score/mw/com/doc/tutorial/chapter_1/provider-tar.tar` resp.
`bazel-bin/score/mw/com/doc/tutorial/chapter_1/consumer-tar.tar`.

To run provider/consumer apps, extract both archives (e.g. in a tmp-directory /tmp/tutorial/chapter_1):

.. code-block:: bash

   mkdir -p /tmp/tutorial/chapter_1
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_1/provider-tar.tar` -C /tmp/tutorial/chapter_1/
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_1/consumer-tar.tar` -C /tmp/tutorial/chapter_1/


... then start the service-provider application in the 1st terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_1/opt/HelloWorldServer
   bin/provider_app


and the service-consumer in the 2nd terminal

.. code-block:: bash

   cd /tmp/tutorial/chapter_1/opt/HelloWorldClient
   bin/consumer_app


.. note::

   Note: The location from where you start the apps/binaries is important. Because the configuration needed for our
   `score::mw::com` middleware is resolved relative to the `cwd`. I.e. the lookup of the configuration is
   `./etc/mw_com_config.json`.

This should produce output in both terminals, which is a mix of log-output from the `score::mw::com library` (since we
provided a logging config `logging.json` configured for console output) and stdout/stderr output from our provider and
consumer app.

If everything went well, you will see the provider app cyclically outputting:
`Sample send completed. Event "message" update sent: Hello World<XXX>`

and the consumer app:
`Sample received. Event "message" update received: Hello World<XXX>`

If no error happens, which lead to termination of provider/consumer app, they both run forever util you kill them.

Service Interface
~~~~~~~~~~~~~~~~~


Our provider application provides a `service instance` according to a specific `service type`. In this case the
service type is `HelloWorldService`, which has a specific service interface.
This service interface is the contract between the provider and the consumer. In many middleware solutions the interface
between the provider and the consumer is described in some specific interface definition language (`IDL`). This makes
a lot of sense due to these main reasons:

1. The middleware may support different programming language bindings. Thus, writing it down in a more abstract language
   and then generating the programming language specific artifacts (interfaces, data types, ...) out of it, is straight
   forward.
2. Even if only a single language binding is supported, there might be restrictions, what datatypes can be exchanged
   between provider and consumer. So even is this case an IDL might make sense as it can force such restrictions easily.

`score::mw::com` currently **doesn't** have an IDL. It was initially implemented with only a C++ language binding
support.
Therefore, an IDL wasn't needed. The service interface needs to be simply written down in C++ following certain
requirements. The benefit: No need for code generators during builds, which often turn out to be a performance
bottleneck.

.. note::

   Note: `score::mw::com` is now also supporting a Rust language binding. In S-CORE communication - since we want to
   support `score::mw::com` based communication between mixed Rust/C++ service providers and consumers - we are already
   designing application binary interface (ABI) compliant data-types. This is essential as we want a LoLa based zero-copy
   data-exchange between Rust and C++, which requires this compatibility on binary level. From these Rust/C++ interop
   use cases the need for an IDL might come up midterm.


This is our simple service interface, which is declared in `hello_world_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/hello_world_service.h>`__:

.. literalinclude:: hello_world_service.h
   :language: cpp
   :lines: 20-28
   :caption: hello_world_service.h


The service interface needs to be defined in the form of a class template, because later a proxy or skeleton side
implementation of the service interface gets generated via instantiation of the service interface template with the
corresponding proxy/skeleton trait. Thus, also the line `using Trait::Base::Base;` is mandatory for each service
interface a user designs.
What is specific to our `HelloWorldInterface` is only the datatype declaration for a `FixedSizeString` and the line:
typename Trait::template Event<FixedSizeString> message{*this, "message"};

This line expresses, that our service interface `HelloWorldInterface` contains an event with name "message".
The datatype of the event is `FixedSizeString`, which we declared in the line above.
The "ugly" template syntax `typename Trait::template` in front of the `Event` type is required by the way, how we
instantiate the service interface with either a `ProxyTrait` or `SkeletonTrait` to get the corresponding proxy or
skeleton class from the interface!

You find the detailed documentation, how such a service interface should be defined in a doxygen comment in
`traits.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/traits.h>`__. There you will also
find the details of how to instantiate a proxy or skeleton class corresponding to the service interface, which we will
discuss below in our HelloWorld example, when looking at the provider/consumer implementations.

Restrictions on datatypes
^^^^^^^^^^^^^^^^^^^^^^^^^


It should be mentioned here: Although the user writes down the service interface and therefore also the datatypes used
in events, fields or methods as plai C++ types, not all types are viable! We do currently have the following
restrictions:

- datatypes must not be polymorphic.
- datatypes must not do dynamic memory allocations
- datatypes need to be default-constructible.

Provider application
~~~~~~~~~~~~~~~~~~~~


Now let's have a look at the implementation of the provider of the `HelloWorldService`. It is
contained in main function of `provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/provider.cpp>`__.

The 1st important step happens in line 30:

.. literalinclude:: provider.cpp
   :language: cpp
   :lines: 30-30
   :caption: provider.cpp


As mentioned in `Service Interface <#service-interface>`__ we create the skeleton side type of the `HelloWorldInterface`
by invoking `AsSkeleton<HelloWorldInterface>` and introduce an alias for it.

.. note::

   Note: The `HelloWorldSkeleton` alias is in this case:

   .. code-block:: cpp

      SkeletonWrapperClass<HelloWorldInterface, SkeletonTrait>
      -> HelloWorldInterface<SkeletonTrait>
      -> SkeletonBase

   where the `HelloWorldInterface<SkeletonTrait>` template instantiation contains the event declaration for the "message"
   event, which is of type `SkeletonEvent<FixedSizeString>`.
   This you could find out by looking into the implementation of the `AsSkeleton` template in
   `traits.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/traits.h>`__ and following the inheritance chain of the generated skeleton class.

Then in line 40 we create an instance of the skeleton class with the `Create` (named constructor) method:

.. literalinclude:: provider.cpp
   :language: cpp
   :lines: 40-40
   :caption: provider.cpp


The argument for the `Create` method is an `instance specifier` (`InstanceSpecifier`), which is basically a string that
is used for a configuration lookup! We will look into the configuration of our HelloWorld example in more detail
later. For now, it is sufficient to understand that the `instance specifier` resolves to a specific configuration for
the service instance, which contains all necessary information for the middleware to create the service instance.

After we successfully created the skeleton instance (the returned `score::core::Result` contains the skeleton instance as
its value in case of success), in line 43 we move the skeleton instance out of the result:

.. literalinclude:: provider.cpp
   :language: cpp
   :lines: 43-43
   :caption: provider.cpp


This is just for convenience, as we want to work with the skeleton instance and not with the result wrapper.
But it also shows a detail here: The `std::move` is needed, because a skeleton instance is not copyable, but only movable.
This is a design decision, which we made, because a skeleton instance holds a lot of internal state and resources, which
we don't want to be copied implicitly! In our design documentation `here <https://github.com/eclipse-score/communication/blob/main/score/mw/com/design/skeleton_proxy/README.md#copymove-support-for-binding-independent-and-dependent-proxiesskeletons>`__
you'll find a more detailed explanation, why skeletons (and also proxies by the way) are not copyable but only movable.

After the call to `OfferService` in line 45 succeeds:

.. literalinclude:: provider.cpp
   :language: cpp
   :lines: 45-45
   :caption: provider.cpp


the service instance is visible for consumers. I.e. they can find the service instance via service-discovery.
Then in line 50 the provider app enters a loop, where it cyclically sends an update for the "message" event.
Since we want to show the zero-copy approach here, this updating/sending of the event is a two-step approach:

Step 1 in line 55 allocates memory for a new event-sample for the "message" event:

.. literalinclude:: provider.cpp
   :language: cpp
   :lines: 55-55
   :caption: provider.cpp

Step 2 in line 74 then signals to score::mw::com, that updating/writing to the memory is done and this new sample
can be made visible to consumers:

.. literalinclude:: provider.cpp
   :language: cpp
   :lines: 74-74
   :caption: provider.cpp


The lines in-between are just about filling the data for the new event in a zero-copy way.
The `Allocate` call in step 1 returns a move-only `SampleAllocateePtr`, pointing to allocated event-slot memory.
The `Send` call in step 2 then moves the ownership of the memory back to `score::mw::com`. Thus, after calling `Send`,
the provider can't access the sample memory anymore.

That's it on the provider side of our HelloWorld example.

Consumer Application
~~~~~~~~~~~~~~~~~~~~


The implementation of the consumer of the `HelloWorldService` is contained in main function of
`consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/consumer.cpp>`__.

Symmetric to the provider side, the 1st important step on the consumer side is the creation of a proxy side type of
the `HelloWorldInterface` via the `AsProxy` template, which we also introduce an alias for:

.. literalinclude:: consumer.cpp
   :language: cpp
   :lines: 29-29
   :caption: consumer.cpp


In the while-loop starting in line 40, the consumer tries to find the service instance of the
`HelloWorldService` via service discovery.

.. literalinclude:: consumer.cpp
   :language: cpp
   :lines: 43-43
   :caption: consumer.cpp


This `FindService` is done based on an `instance specifier` (`InstanceSpecifier`), which is again a lookup key for the
configuration. I.e. in the configuration file, which we will have a deeper look into later, there is an item with this key,
which details the configuration for the service instance and is needed for the fin-process.
That we are using the exact same key `MyHelloWorldServiceInstance` as the provider side did for creation is because we
are lazy and re-use the same configuration for provider and consumer in our example.

The `FindService` call used here is a **synchronous** call. It does a service-discovery lookup and either returns an empty
`ServiceHandleContainer` (in case no matching service instance was found) or a `ServiceHandleContainer` with a valid service
handle, which can be used to create a proxy instance for the service instance. The result of a `FindService` is a container
structure, because `score::mw::com` supports searches/`FindService`, which can return multiple service instances!

Our consumer app repeatedly searches for the service instance until it is available. Thus, if the returned
`ServiceHandleContainer` is empty, the consumer app waits for 1 second and then tries again to find the service instance.

If the `ServiceHandleContainer` contains a valid service handle (service instance has been found), we can create a proxy
instance for the service instance via the `Create` method of the `HelloWorldProxy`. This is done in line 58:

.. literalinclude:: consumer.cpp
   :language: cpp
   :lines: 58-58
   :caption: consumer.cpp


After this method returned a valid result containing the proxy instance, we now have our consumer side representation
of the service instance, we can interact with.
Symmetrically to the provider side, we move the proxy instance out of the result wrapper for convenience in line 65:

.. literalinclude:: consumer.cpp
   :language: cpp
   :lines: 65-65
   :caption: consumer.cpp


Just like the skeleton instance, a proxy instance is also not copyable but only movable, which is the reason for the
`std::move` here.
Before being able to access samples of the "message" event, we need to **subscribe** to the event **first**. This is done in
line 66:

.. literalinclude:: consumer.cpp
   :language: cpp
   :lines: 66-66
   :caption: consumer.cpp


`ProxyEvent<T>::Subscribe` expects an argument `max_sample_count`. With this argument (integer) the caller announces,
how many event samples – he is going to acquire later via the `GetNewSamples` API – he is going to hold/keep
in parallel at maximum. This call could fail, if the `max_sample_count` is too high and the provider can't provide
that many samples in parallel, because he hasn't reserved enough resources in its configuration, which we will have a
look at later.

After the `Subscribe` call succeeded (returned `Result<void>` contains no error), our consumer app now enters a loop
(in line 75), where it cyclically checks for updates for the "message" event.

The API to do this, is the `ProxyEvent::GetNewSamples` API. You see the call in line 78:

.. literalinclude:: consumer.cpp
   :language: cpp
   :lines: 78-84
   :caption: consumer.cpp


.. note::

   Note: In our example, we do a somewhat **dirty** shortcut, by directly calling `GetNewSamples` after the subscription.
   As per the API contract of `GetNewSamples()` this might fail with an error `ComErrc::kNotSubscribed` in case the
   subscription state of the event was not even once in state `SubscriptionState::kSubscribed`.
   We will look into the subscription state machine in more detail in a later chapter and show, how to access the current
   state. Here we also take benefit of the fact, that for our current LoLa binding, we know, that after a successful
   subscription call, the subscription state is immediately in a state, where `GetNewSamples` can be called...


The `GetNewSamples` API requires two arguments. The first argument is a callback, which `score::mw::com` will call once
for each newly available (since the last call to `GetNewSamples`) sample.
The second argument `max_num_samples` can be used to restrict the number of calls to the callback and, thus the number of
new samples being retrieved. In case of our simple `HelloWorld` consumer app, we set this argument to `1`, because we are
only interested in the latest sample and – see our subscribe call above – we also only subscribed for a single
sample. (Just accept this short explanation for now, in upcoming chapters we will dig a bit deeper into the relation between
this `max_num_samples` argument and the `max_sample_count` argument of the `Subscribe` call.)

The 1st argument (the callback) needs to be a callable with the following signature:

.. code-block:: cpp

   void(SamplePtr<const SampleType>)


, where `SampleType` is the datatype of the event, which in our case is `FixedSizeString`.

In our example, we use a lambda function as callback, which prints the received message to the console.
The `SamplePtr<T>` is a move-only smart pointer, which points to the sample memory. As discussed above:
How many `SamplePtr` instances can be held in parallel is limited by the `max_sample_count` argument of the `Subscribe`
call.

In our lambda callback, we do not move the `SamplePtr` out of the callback, but let it go out of scope at the end of the
callback. This is important: If we would move the `SamplePtr` to some outer scope, where it would still be alive, when
the next `GetNewSamples` call is done, you would get an error `ComErrc::kMaxSamplesReached`, because the number of
`SamplePtr` instances alive in parallel matches `max_sample_count` of `1` of our subscription. I.e. `score::mw::com` can't
create another `SamplePtr` for a new sample to hand over to the callback! So, in this error case, the callback won't
be called and `GetNewSamples` will return an error `ComErrc::kMaxSamplesReached`. In the good case `GetNewSamples` retuns the
number of new samples, which were handed over to the callback.

Configuration
~~~~~~~~~~~~~


Every `score::mw::com` application needs a configuration. The configuration is a JSON file, which is resolved relative to
the current working directory of the application. The essential parts within the configuration are the service instance
configurations. This relates to both roles a service instance can have: A provided service instance (skeleton) and a
consumed service instance (proxy).
In our `HelloWorld` example, we have a single service instance configuration, which is used by both the provider and the
consumer. The configuration is located in `mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/mw_com_config.json>`__ and looks like this:

.. literalinclude:: mw_com_config.json
   :language: json
   :lines: 23-46
   :caption: mw_com_config.json


You see, that this service instance is identified (using the property `instanceSpecifier`) as `MyHelloWorldServiceInstance`
in the config. This is exactly the same instance specifier we used in both - provider and consumer app.
So, the provider app did find all necessary configuration for **creating** the service instance (skeleton view) via this
`MyHelloWorldServiceInstance` instance-specifier lookup, while the consumer app did find all necessary configuration for
**finding** the service instance and creating the proxy view via the same instance-specifier lookup.

The configuration is generally structured into the definition of service types and then concrete service instances,
which refer to a service type. So this is semantically a 1:N relation. We can have multiple service instances of the
same service type. In our example, we have only a single service instance of the `HelloWorldService` service type.
You also see in this config snippet above, that the service instance identified by the `instanceSpecifier`
`MyHelloWorldServiceInstance` refers to the service type `HelloWorldService` via the property `serviceTypeName`. More
concise, it refers to the service type via its fully qualified name, which is `/score/mw/com/tutorial/HelloWorldService`.
If you scroll up in the `mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_1/mw_com_config.json>`__ file, you will find the definition of the service type
`HelloWorldService` with its fully qualified name `/score/mw/com/tutorial/HelloWorldService`.

This should be enough for now. There is a specific `configuration documentation <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/configuration/README.md>`__,
which goes into more detail about the configuration structure and its semantics.

Summary
^^^^^^^


A short summary of what we have learned in this chapter:

- The service interface is defined in terms of a C++ struct, which has to follow a certain pattern, which is documented
  in `traits.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/traits.h>`__.
- The provider side skeleton data type is generated via the `AsSkeleton` template, which is instantiated with the service
interface.
- The consumer side proxy data type is generated via the `AsProxy` template, which is instantiated with the service
interface.
- For both, the provider and the consumer, the service instance is identified via an `instance specifier`, which is a
  string that is used for a configuration lookup.
- The provider creates a skeleton instance via the `Create` method of the skeleton data type and then offers the service
instance via the `OfferService` method.
- The consumer finds the service instance via the `FindService` method of the proxy data type and then creates a proxy
instance via the `Create` method of the proxy data type.
- The consumer, which wants to receive event samples, the provider sends out, 1st needs to subscribe to the event via
the `Subscribe` method of the proxy event data type.
- The consumer then can retrieve new samples via the `GetNewSamples` method of the proxy event data type, which takes a
callback as argument, which is called for each new sample.
