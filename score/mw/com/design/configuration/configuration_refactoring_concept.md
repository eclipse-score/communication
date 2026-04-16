# Configuration Refactoring Concept

## Motivation & Scope

The current configuration parsing system in `mw::com` is a monolithic JSON-only parser (`config_parser.cpp`)
that interleaves format parsing, cross-validation, and binding-specific logic in a single ~1450-line file.
This design has served us well for the initial implementation, but several drivers now require a refactoring:

1. **Multi-format support**: BMW series projects require JSON configuration, while future deployments
   will use FlatBuffer for performance on resource-constrained targets. Both formats must be supported
   in parallel.
2. **ASIL qualification**: FlatBuffer parsing cannot be used in safety-critical paths until ASIL
   qualification is complete. A feature flag is needed to gate FlatBuffer usage.
3. **Separation of concerns**: Parsing (format translation) and validation (semantic correctness checks)
   are currently interleaved, making it difficult to extend either independently.
4. **Proxy/Skeleton-specific validation**: Construction-time checks specific to proxy or skeleton
   usage patterns are not currently supported.
5. **Debuggability**: On-target debugging requires a human-readable format (JSON), even when FlatBuffer
   is the primary format.

### Out of Scope

Implementation is initiated in work item #98800. This document defines the
concept and architecture that, the following work items will implement.

### Related Work

- [Community Discussion](https://github.com/orgs/eclipse-score/discussions/2459): CFT Weekly alignment
  on configuration refactoring approach
---

## BMW-Alignment Decisions (22.01.2026)

The following decisions were made during the BMW-alignment meeting on 22.01.2026 and are incorporated
into this concept:

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | **JSON and FlatBuffer supported in parallel** | BMW projects use JSON today; FlatBuffer needed for production targets. Both must coexist. |
| 2 | **Phased rollout** | Phase 1: format/parsing refactoring. Phase 2: validation refactoring. Reduces risk. |
| 3 | **Feature flag for FlatBuffer** | FlatBuffer parsing gated until ASIL qualification is complete. Default remains JSON. |
| 4 | **Separation of parsing from validation** | Parsing translates format to C++ objects. Validation checks semantic correctness. These are independent concerns. |
| 5 | **On-target human-readable format (JSON) for debugging** | Even when FlatBuffer is the primary format, JSON must remain available on-target for debugging and diagnostics. |

---

## Resolved Open Questions from PI-1

### 1. String-View Handling in FlatBuffer Context

**Decision: Copy on parse**

The parsing strategy copies all string data into owning `std::string` objects when constructing
the `Configuration`. The FlatBuffer is not kept alive after parsing completes.

**Rationale:**

- The `Configuration` object is a singleton that lives for the entire program lifetime. It is
  stored in a static context by `Runtime` and its internal maps are immutable after construction.
  There is no benefit to maintaining views into a FlatBuffer when the data must persist anyway.

- The current codebase uses `std::string` pervasively: `ServiceIdentifierType` stores an owning
  `std::string name_`, and all event/field/method maps use `std::unordered_map<std::string, ...>`.
  Introducing `std::string_view` would require changes throughout the type hierarchy and would
  create an asymmetry between JSON and FlatBuffer strategies.

- The primary benefit of FlatBuffer in this context is **compact on-disk representation** and
  **fast deserialization** (no intermediate JSON DOM construction), not zero-copy string access.
  The number of strings in a typical configuration is small (tens to low hundreds), so the copy
  overhead is negligible compared to the overall startup cost.

- Both `ConfigurationJsonParsingStrategy` and `ConfigurationFlatbufParsingStrategy` produce
  identical `Configuration` objects with owned data. This makes the strategies fully interchangeable
  and simplifies testing.

**Consequence:** The FlatBuffer buffer (`std::vector<uint8_t>`) can be freed immediately after
parsing completes. The parsing strategy does not need to outlive the `Configuration` object.

```
FlatBuffer file   -->  [FlatBuf Strategy]  --copies-->  Configuration (owns std::string)
                                                         |
JSON file         -->  [JSON Strategy]     --copies-->   | (identical object)
                                                         v
                                                    Buffer freed
```

### 2. Constructor Validation for Skeleton & Proxy

**Decision: Layered validation with three phases**

Validation is separated into three layers, each with a distinct responsibility and execution point:

#### Phase 1: Format Validation (during parsing)

Structural and syntactic validation that ensures the configuration file is well-formed.

- **JSON**: JSON schema validation (already exists via `mw_com_config_schema.json`)
- **FlatBuffer**: FlatBuffer verifier (built-in to FlatBuffers library)
- **Scope**: Field types, required fields, structural integrity
- **Failure mode**: Fatal termination with descriptive error message (current behavior preserved)

#### Phase 2: Generic Rule Validation (after parsing, before use)

Semantic cross-checks that apply regardless of whether the configuration is used by a proxy or
skeleton. These rules are currently embedded in `config_parser.cpp` and will be extracted into
dedicated validation functions.

Current rules that move to this phase:

| Rule | Current Location | Description |
|------|-----------------|-------------|
| ASIL level consistency | `CrosscheckAsilLevels()` | Service instance ASIL cannot exceed process ASIL |
| Type-instance reference integrity | `CrosscheckServiceInstancesToTypes()` | All service instances must reference a defined service type |
| Binding consistency | `CrosscheckServiceInstancesToTypes()` | Binding type in instance must match binding type in referenced type |
| Service element existence | `CrosscheckServiceInstancesToTypes()` | Events/fields in instance deployments must exist in referenced type deployment |
| Unique event/field/method IDs | `AreEventFieldAndMethodIdsUnique()` | No duplicate IDs within a service type |
| Duplicate event/field names | `ServiceElementInstanceDeploymentParser` | No duplicate event or field names within an instance |

**Failure mode**: Fatal termination with descriptive error message (current behavior preserved).

#### Phase 3: User-Specific Validation (at Proxy/Skeleton construction)

Validation specific to the usage context. This is performed when a `ProxyServiceInstanceConfiguration`
or `SkeletonServiceInstanceConfiguration` view is constructed, typically during proxy or skeleton
instantiation.

Examples of skeleton-specific checks:
- Offered service instance configuration is complete (all required events/fields/methods present)
- Provider permissions (`allowedProvider`) are configured if strict permission mode is enabled

Examples of proxy-specific checks:
- Required service instance is properly configured for consumption
- Consumer permissions (`allowedConsumer`) are configured if strict permission mode is enabled

**Failure mode**: Returns error result (not fatal), allowing the caller to handle gracefully.

```
                    +-----------------------+
                    | Phase 1: Format       |
                    | (during parsing)      |
                    | - JSON schema         |
                    | - FlatBuf verifier    |
                    +-----------+-----------+
                                |
                                v
                    +-----------------------+
                    | Phase 2: Generic Rules|
                    | (after parsing)       |
                    | - ASIL cross-check    |
                    | - Type-instance refs  |
                    | - ID uniqueness       |
                    +-----------+-----------+
                                |
                                v
                    +-----------------------+
                    |  Configuration        |
                    |  (immutable database) |
                    +-----------+-----------+
                               / \
                              /   \
                             v     v
              +----------------+ +-------------------+
              | Skeleton View  | | Proxy View        |
              | Phase 3 checks | | Phase 3 checks    |
              | - offered inst | | - required inst   |
              | - provider perm| | - consumer perm   |
              +----------------+ +-------------------+
```

---

## Architecture Overview

### Strategy Pattern for Configuration Parsing

The monolithic `Parse()` functions in `config_parser.h` are replaced by a strategy interface
that abstracts the configuration file format:

```cpp
namespace score::mw::com::impl::configuration
{

/// Interface for configuration parsing strategies.
/// Each strategy handles a specific file format (JSON, FlatBuffer)
/// and produces an identical Configuration object.
class IConfigurationParsingStrategy
{
  public:
    virtual ~IConfigurationParsingStrategy() = default;

    /// Parse configuration from the given file path.
    /// @param path Path to the configuration file
    /// @return Parsed Configuration object with all data copied/owned
    virtual Configuration Parse(std::string_view path) = 0;
};

}  // namespace score::mw::com::impl::configuration
```

Two concrete strategies implement this interface:

**`ConfigurationJsonParsingStrategy`**: Wraps the existing JSON parsing logic from
`config_parser.cpp`. This is the default strategy and preserves current behavior exactly.

**`ConfigurationFlatbufParsingStrategy`**: Parses FlatBuffer-encoded configuration files.
Copies all string data into `std::string` during parsing (see [String-View Handling decision](#1-string-view-handling-in-flatbuffer-context)).
Gated behind a feature flag.

Both strategies produce identical `Configuration` objects. The strategy is selected at
`Runtime::Initialize()` time based on the feature flag and/or file extension.

### Configuration as Database

The `Configuration` class remains the central data store, largely unchanged from the current
implementation:

```cpp
class Configuration final
{
  public:
    using ServiceTypeDeployments = std::unordered_map<ServiceIdentifierType, ServiceTypeDeployment>;
    using ServiceInstanceDeployments = std::unordered_map<InstanceSpecifier, ServiceInstanceDeployment>;

    // Constructor, getters unchanged from current implementation
    // ...

    const GlobalConfiguration& GetGlobalConfiguration() const noexcept;
    const TracingConfiguration& GetTracingConfiguration() const noexcept;
    const ServiceTypeDeployments& GetServiceTypes() const noexcept;
    const ServiceInstanceDeployments& GetServiceInstances() const noexcept;

  private:
    ServiceTypeDeployments service_types_;
    ServiceInstanceDeployments service_instances_;
    GlobalConfiguration global_configuration_;
    TracingConfiguration tracing_configuration_;
};
```

Key properties:
- **Immutable after construction**: Maps are populated during parsing and never modified.
  `AddServiceTypeDeployment()` and `AddServiceInstanceDeployments()` remain for dynamic
  additions during runtime (e.g., for testing or service discovery).
- **Singleton lifetime**: Stored in `Runtime`'s static context for program lifetime.
- **Pointer stability**: Because maps are not reordered after construction, pointers to
  contained objects remain valid.

### Typed Views for Proxy/Skeleton

New view classes provide context-specific access to the `Configuration` with additional
validation at construction time:

```cpp
/// Read-only view for skeleton-specific configuration access.
/// Validates that the configuration is suitable for offering a service.
class SkeletonServiceInstanceConfiguration
{
  public:
    /// Constructs a skeleton view and validates skeleton-specific constraints.
    /// @param config Reference to the parsed Configuration
    /// @param instance_specifier The InstanceSpecifier for this skeleton
    /// @return View on success, error if validation fails
    static score::Result<SkeletonServiceInstanceConfiguration> Create(
        const Configuration& config,
        const InstanceSpecifier& instance_specifier);

    const ServiceInstanceDeployment& GetServiceInstanceDeployment() const noexcept;
    const ServiceTypeDeployment& GetServiceTypeDeployment() const noexcept;

  private:
    SkeletonServiceInstanceConfiguration(
        const ServiceInstanceDeployment& instance_deployment,
        const ServiceTypeDeployment& type_deployment);

    const ServiceInstanceDeployment& instance_deployment_;
    const ServiceTypeDeployment& type_deployment_;
};

/// Read-only view for proxy-specific configuration access.
/// Validates that the configuration is suitable for consuming a service.
class ProxyServiceInstanceConfiguration
{
  public:
    static score::Result<ProxyServiceInstanceConfiguration> Create(
        const Configuration& config,
        const InstanceSpecifier& instance_specifier);

    const ServiceInstanceDeployment& GetServiceInstanceDeployment() const noexcept;
    const ServiceTypeDeployment& GetServiceTypeDeployment() const noexcept;

  private:
    ProxyServiceInstanceConfiguration(
        const ServiceInstanceDeployment& instance_deployment,
        const ServiceTypeDeployment& type_deployment);

    const ServiceInstanceDeployment& instance_deployment_;
    const ServiceTypeDeployment& type_deployment_;
};
```

These views:
- **Do not own data**: They hold references into the `Configuration` singleton.
- **Validate at construction**: The `Create()` factory method performs Phase 3 validation.
- **Return errors, not fatal**: Unlike Phase 1/2 validation, proxy/skeleton validation returns
  a `score::Result` to allow graceful error handling by the caller.

### Validation Separation

Generic validation rules are extracted from `config_parser.cpp` into a separate component:

```cpp
namespace score::mw::com::impl::configuration
{

/// Validates a parsed Configuration for semantic correctness.
/// Called after parsing, before the Configuration is made available.
/// Terminates on validation failure (fatal errors).
void ValidateConfiguration(const Configuration& config);

}  // namespace score::mw::com::impl::configuration
```

This function replaces the current inline calls to `CrosscheckAsilLevels()` and
`CrosscheckServiceInstancesToTypes()` at the end of `Parse()`. The validation logic itself
is unchanged; only its location moves.

---

## Phased Rollout Plan

### Phase 1: Format & Parsing (Initial work in #98800)

**Goal**: Introduce the strategy pattern and FlatBuffer support without changing validation behavior.

1. Define `IConfigurationParsingStrategy` interface
2. Extract current JSON parsing logic into `ConfigurationJsonParsingStrategy`
   - Internal refactoring only; external behavior unchanged
   - Current `Parse()` free functions become thin wrappers that delegate to the JSON strategy
3. Implement `ConfigurationFlatbufParsingStrategy` behind feature flag
   - String data copied on parse (copy-on-parse decision)
   - Produces identical `Configuration` objects as JSON strategy
4. Update `Runtime::Initialize()` to select strategy based on feature flag
5. Preserve backward compatibility: default behavior is JSON parsing, identical to current

### Phase 2: Validation & Views (Work Items shall be planned in #98800)

**Goal**: Separate validation from parsing and introduce typed views.

1. Extract generic validation from `config_parser.cpp` into `ValidateConfiguration()`
   - Move `CrosscheckAsilLevels()`, `CrosscheckServiceInstancesToTypes()`,
     `AreEventFieldAndMethodIdsUnique()`, and duplicate-name checks
   - Validation called by `Runtime::Initialize()` after parsing, before exposing Configuration
2. Implement `SkeletonServiceInstanceConfiguration` with skeleton-specific validation
3. Implement `ProxyServiceInstanceConfiguration` with proxy-specific validation
4. Integrate typed views into skeleton and proxy constructors

---

## Feature Flag Design

FlatBuffer parsing is gated by a feature flag until ASIL qualification is complete:

- **Mechanism**: Compile-time preprocessor define (`SCORE_MW_COM_ENABLE_FLATBUF_CONFIG`)
- **Default**: Not defined (JSON is the default and only available strategy)
- **When defined**: FlatBuffer strategy is compiled in and can be selected at runtime via
  file extension (`.json` -> JSON strategy, `.bfbs`/`.bin` -> FlatBuffer strategy)
- **Bazel integration**: Controlled via a Bazel `config_setting` and `select()` in BUILD files

```python
# In BUILD file
cc_library(
    name = "config_parser",
    deps = [
        ":configuration_json_parsing_strategy",
    ] + select({
        "//score/mw/com:flatbuf_config_enabled": [
            ":configuration_flatbuf_parsing_strategy",
        ],
        "//conditions:default": [],
    }),
)
```

This ensures that FlatBuffer dependencies are not even linked when the feature flag is off,
keeping the binary size unchanged for non-FlatBuffer deployments.

---

## Structural View

See [structural_view_refactored.puml](structural_view_refactored.puml) for the complete class diagram.

<img alt="STRUCTURAL_VIEW_REFACTORED" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/configuration/structural_view_refactored.puml">

## Sequence View: Parsing & Validation

See [sequence_parsing_view.puml](sequence_parsing_view.puml) for the parsing and validation sequence.

<img alt="SEQUENCE_PARSING_VIEW" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/configuration/sequence_parsing_view.puml">
