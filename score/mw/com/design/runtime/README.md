# Runtime

Our `mw::com` (`ara::com`) implementation has a `mw::com::impl::Runtime`, which implements a singleton pattern.
It is used/needed as we need a single entry-point and also "resource holder" per process for our implementation.
The Runtime aspect is interwoven with our configuration approach, therefore you will see references to `Runtime`
already in [configuration design](../configuration/README.md).

## Initialization
Our `impl::Runtime` singleton gets either created/initialized with one of the following static methods:
- `static void Initialize()` // uses configuration file from a standardized location
- `static void Initialize(int argc, const char** argv)` // uses configuration file as per cmd line args
- `static void Initialize(std::string const&)` // for unit-testing: json-config is handed over as a string

or indirectly via a call to `getInstance()`, which internally calls `Initialize()`, if the singleton instance wasn't yet
created.

## Binding specific Runtime extensions
All concrete technical bindings will have their own runtime extension as they might need different central/common
resources. Which technical bindings are used within one `mw::com` (`ara::com`) enabled process, depends on the
configuration.
Since the configuration gets already read-in/parsed by the binding independent `impl::Runtime`, it is logical, that the
creation/instantiation of binding specific runtimes is done by `impl::Runtime` with the help of
`impl::RuntimeBindingFactory`, which creates instances of `impl:RuntimeBinding`.

The class diagram of this design is as follows:

<img alt="RUNTIME_STRUCTURAL_VIEW" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/runtime/runtime_structural_view.puml">

Since `impl::IRuntimeBinding` is only a **_very coarse grained_** interface and binding specific runtimes will have each
very specific methods/types, the binding specific code, which needs to access its specific binding runtime needs to do
a "downcast", when it gets a `IRuntimeBinding` instance from the binding independent `impl::Runtime`. The sequence is
shown in the following sequence diagram:

<img alt="RUNTIME_SEQUENCE_VIEW" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/runtime/runtime_sequence_view.puml">

## Static dependencies
Binding specific runtimes might use infrastructure in the form of static instances. E.g. the `lola::Runtime` uses
indirectly `score::os::Mqueue::instance()`, which is also a static singleton. Sometimes (binding)parts of `mw::com` have the
expectation, that such used infrastructure "singletons" are still there/accessible during destruction of themselves!

There we basically run into the C++ "_static initialization fiasco_" scenario. I.e. when not explicitly taken care of,
we could run into the situation, that our static singleton `impl::Runtime` and it's dependent `impl::IRuntimeBinding`s
get destroyed after the infrastructure static singletons, but accesses them during destruction. To avoid this, we
have to make sure, that static infrastructure singletons are created/used **before** our `mw::com` static singletons are
created! Then the C++ runtime/compiler makes sure, that these infrastructure static singletons are destroyed **after** our
`mw::com` parts.

This is the reason, why `impl::lola::Runtime` contains a static `InitializeStaticDependencies()` method. This will
be called by `impl::RuntimeBindingFactory` before it constructs a singleton instance of `impl::lola::Runtime`.
The implementation of `impl::lola::Runtime::InitializeStaticDependencies()` makes sure to prematurely
instantiate/initialize those static infrastructure singletons (like in this case `score::os::Mqueue::instance()`), so that
it is assured, they will exist also in its destruction phase!
