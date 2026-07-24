Chapter 2: Hello World configuration refined
============================================


Our HelloWorld example from chapter 1 had a very "sloppy" configuration approach. In this chapter we will refine the
configuration and show, what possibilities `score::mw::com` provides to load configurations.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_2`. It contains the following
files:

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/BUILD>`__
     - This file contains bazel targets for this example.
   * - `consumer/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/consumer/consumer.cpp>`__
     - Implementation of the service consumer app. The `main()` for the consumer
   * - `consumer/consumer.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/consumer/consumer.h>`__
     - Header (empty - we just always want to have cpp/h pairs) of the service consumer.
   * - `consumer/consumer_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/consumer/consumer_config.json>`__
     - This file contains the configuration for `score::mw::com` for the consumer app.
   * - `consumer/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/consumer/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/provider/provider.cpp>`__
     - Implementation of the service provider.
   * - `provider/provider.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/provider/provider.h>`__
     - Header (empty) of the service provider.
   * - `provider/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/provider/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/provider/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the provider app.
   * - `hello_world_service.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/hello_world_service.cpp>`__
     - This file is empty as the service interface is completely defined in the header.
   * - `hello_world_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/hello_world_service.h>`__
     - This file contains the definition of the service interface.



----------------------------------------

How to run the example
~~~~~~~~~~~~~~~~~~~~~~


The steps are almost the same as in chapter 1 (minus the chapter adaptions in paths)! We will explicitly mark the
relevant differences here.
To run the example (on host/Linux), you can use the following bazel commands:

.. code-block:: bash

   # Build the provider and consumer targets
   bazel build //score/mw/com/doc/tutorial/chapter_2:provider-tar 
   bazel build //score/mw/com/doc/tutorial/chapter_2:consumer-tar


The bazel build above creates tar-archives for the provider application and the consumer application.
They should then be located under `bazel-bin/score/mw/com/doc/tutorial/chapter_2/provider-tar.tar` resp.
`bazel-bin/score/mw/com/doc/tutorial/chapter_2/consumer-tar.tar`.

To run provider/consumer apps, extract both archives (e.g. in a tmp-directory):

.. code-block:: bash

   mkdir -p /tmp/tutorial/chapter_2
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_2/provider-tar.tar` -C /tmp/tutorial/chapter_2/
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_2/consumer-tar.tar` -C /tmp/tutorial/chapter_2/


... then start the service-provider application in the 1st terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_2/opt/HelloWorldServer
   bin/provider_app


and the service-consumer in the 2nd terminal (this is notably different to chapter 1, because we now provide a specific
configuration for the consumer app, which is loaded via the `--service_instance_manifest` argument):

.. code-block:: bash

   cd /tmp/tutorial/chapter_2/opt/HelloWorldClient
   bin/consumer_app --service_instance_manifest ./etc/consumer_config.json 


Provider application
~~~~~~~~~~~~~~~~~~~~


The code for the provider application is **unchanged** compared to chapter 1. The only difference is the configuration,
which is now split into a provider specific configuration and a consumer specific configuration.
However, the provider specific configuration is still the same as in chapter 1.

Consumer application
~~~~~~~~~~~~~~~~~~~~


The consumer application is also almost identical to the consumer application of chapter 1. The only difference is, that
we now load a specific configuration! So, if you look at line 37 of `consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/consumer/consumer.cpp>`__ file,
you will see that we added a specific call:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 37-37
   :caption: consumer/consumer.cpp


So, there is a specific call to initialize the `score::mw::com` runtime, which takes the command line arguments of the
consumer app as input. And when you look, how we start the consumer app in this chapter, you will see that we provide a
specific argument `--service_instance_manifest ./etc/consumer_config.json` to the consumer app.
I.e. the `--service_instance_manifest` argument provides a mechanism to give the path to a specific configuration file.

Configuration
~~~~~~~~~~~~~


Loading of configurations
^^^^^^^^^^^^^^^^^^^^^^^^^


Here are the details regarding loading of configurations for `score::mw::com` applications:

Typically – as you have seen in our HelloWorld example in chapter 1 – there is **no need** for an explicit
call to `score::mw::com::runtime::InitializeRuntime()`. The configuration gets **implicitly** loaded by `score::mw::com`,
when it is needed. This happens within the 1st call to a `score::mw::com` API, which needs the configuration.

This implicit/automatic loading expects a configuration file named `mw_com_config.json` to be located in the path
`./etc` relative to the current working directory of the application.
In the HelloWorld example of chapter 1, we did exactly that. We provided a single configuration file named
`mw_com_config.json` in the `./etc` path relative to the current working directory of the provider and consumer app.
Same we did in chapter 2 for the provider app. But for the consumer app, we now provide a specific configuration file
named `consumer_config.json` in the `./etc` path relative to the current working directory of the consumer app. This is
why we need to explicitly call `score::mw::com::runtime::InitializeRuntime(argc, argv)` in the consumer app, to load this
specific configuration file.

If you commented out the `score::mw::com::runtime::InitializeRuntime(argc, argv)` call in the consumer app, you would 
run into an error like this, when starting the consumer app:

.. code-block:: text

   q240165@cmucw1079481:/tmp/tutorial/chapter_2/opt/HelloWorldClient$ bin/consumer_app --service_instance_manifest ./etc/consumer_config.json 
   2026/07/02 10:43:53.1833727 39169650 000 ECU1 EXP1 lola log fatal verbose 7 Parsing config file ./etc/mw_com_config.json failed with error: An error occurred during parsing. :  Failed to open file  . Terminating. 
   Aborted (core dumped)


So here are some best practices regarding configuration files for `score::mw::com` applications:

- typically go for the default/implicit approach: Put the configuration file `mw_com_config.json` in the `./etc` path
relative to the current working directory of the application.
- if you have a strong reason to have a different configuration file name or path, then go for the explicit approach:
  - either call `score::mw::com::runtime::InitializeRuntime(argc, argv)` in your application and provide the path to the
    configuration file via the `--service_instance_manifest` command line argument.
  - or - for the real "power-user" - call `score::mw::com::runtime::InitializeRuntime(const RuntimeConfiguration& runtime_configuration)`,
    where you can provide a `RuntimeConfiguration` object, which allows you to set the path to the configuration file programmatically.
- when you decide to go for the explicit approach, you should do the loading of the configuration as early as possible in
  your application, avoid the implicit loading of the configuration by `score::mw::com` to kick in! Because, once a
  configuration is loaded, it can't be changed anymore. Any subsequent call to `score::mw::com::runtime::InitializeRuntime()`
  will return an error, because the configuration is already loaded and can't be changed anymore.

Separate configuration files for provider and consumer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


The even bigger change between our HelloWorld example of chapter 1 and the HelloWorld example of chapter 2 is, that we
now have **separate** configuration files for the provider and the consumer! This is the expected approach for real-world
applications. Sharing a single configuration file between provider and consumer is error-prone and was only a shortcut
for our HelloWorld example!

If you looked closer at the log output of both the provider and the consumer app in our HelloWorld example from
chapter 1, you would see something like this:

.. code-block:: text

   2026/07/02 15:21:24.8484053 205672912 000 ECU1 EXP1 lola log info verbose 2 No explicit applicationID configured. Falling back to using process UID. Ensure unique UIDs for applications using mw::com.


The root cause for this log message is, that both the provider and the consumer app used the same configuration file,
which did not contain a `global` section with an `applicationID` property! The `global`section is a top-level section in
the configuration file, which holds application-wide configuration properties. This section is optional, since all its
properties do have more or less sensible defaults. One of these properties is the `applicationID`, which is a unique
identification for a `score::mw::com application`. If this property is not set in the configuration, `score::mw::com`
will fall back to using the process UID as application ID. But this then implicitly requires, that if you start the
same executable (using the same configuration without an explicitly set `applicationID`) multiple times, you need to
ensure that each process has a **unique UID**!

This is not the case for our HelloWorld example of chapter 1, because we started the provider and the consumer app under
the **same user**. Thus, this already opens the door for **potential problems**, which are extremely hard to debug!

.. note::

   In the specific case of our HelloWorld example in chapter 1, it is luckily not a problem, because we have a single
   provider and a single consumer falsely sharing the same `applicationID`. But if we would have multiple consumers, which
   all share the same `applicationID` and consume the same service instance, **we would run into problems**!
   Hint: Each of the consuming applications stores some information within the shared-memory control-section of the
   provided service instance, needed for error-handling and restart control. The key for this storage is the
   `applicationID` of the consuming application. If multiple consumers share the same `applicationID`, they will overwrite
   each other's information in the shared-memory control-section of the service instance!

That means: The `global` section, which is optional in theory but in practice **should always be present** in the
configuration file, needs to be application specific. So this basically rules out sharing a configuration file between
`score::mw::com` applications.

Another benefit of having separate configuration files for provider and consumer is, that you can be more concise in the
configuration file!

Let's now just compare the configuration files of the provider and the consumer app in our HelloWorld example of
chapter 2 - here especially the service-instance deployment part!

This is, how the provider app configuration looks like in `mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/provider/mw_com_config.json>`__
with regard to the service instance deployment:

.. literalinclude:: provider/mw_com_config.json
   :language: json
   :lines: 39-40
   :caption: provider/mw_com_config.json


Because the provider app is responsible for the resource allocations, it needs to know, how many sample slots it needs
to reserve for the "message" event and how many maximum subscribers it needs to support for the "message" event!

If you now look at the consumer app configuration in `consumer_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_2/consumer/consumer_config.json>`__
with regard to the service instance deployment, you will see, that it is much smaller and more concise:

.. literalinclude:: consumer/consumer_config.json
   :language: json
   :lines: 35-35
   :caption: consumer/consumer_config.json


As you see, the consumer app configuration does not need to know about the number of sample slots and the maximum number
of subscribers for the "message" event. This is because the consumer app does not need to allocate any resources for the
service instance. So it does not need to define these properties in the configuration!
In essence this has to do with the fact, that you always look at the configuration either in the role of a provider or
in the role of a consumer. Depending on your role, different properties are relevant and need to be defined in the
configuration.
This is explained in detail in the configuration documentation `here <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/configuration/README.md#role-dependent-view>`__.

Summary
~~~~~~~


A short summary of what we have learned in this chapter:

- Don't share configuration files between score::mw::com applications. Each application should have its own
  configuration file with explicitly set `applicationID` in the `global` section.
- When possible: Use the default/implicit configuration loading approach, which expects a configuration file named
  `mw_com_config.json` in the `./etc` path relative to the current working directory of the application.
- If you need to use a different configuration file name or path, do the explicit loading/initialization right at the
  beginning of your application (e.g. via `score::mw::com::runtime::InitializeRuntime(argc, argv)`) before any other
  `score::mw::com` API is called.
