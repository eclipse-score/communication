# Error Handling Guide

This document provides comprehensive information about error codes, messages, and recovery strategies for all mw::com public APIs.

## Error Classification

- **Recoverable**: Error can be resolved through retry, re-subscription, or alternative approaches
- **Potentially Recoverable**: May be recoverable depending on context and timing
- **Non-Recoverable**: Indicates configuration, programming, or infrastructure issues that cannot be resolved at runtime

---

## Public API Error Analysis

### Runtime APIs

#### `score::mw::com::runtime::ResolveInstanceIDs()`

| API | Error | Error Description | Error Handling |
|-----|-------|------------------|----------------|
| `ResolveInstanceIDs()` | `kInstanceIDCouldNotBeResolved` | Runtime could not resolve a valid InstanceIdentifier from the provided InstanceSpecifier. | **Non-Recoverable**: Configuration error in service manifest. Service not defined in JSON configuration file. <br> **Fix**: Update manifest configuration - this is a deployment issue, not runtime issue. |

------

### Instance Identifier APIs

#### `score::mw::com::InstanceIdentifier::Create()`

| API | Error | Error Description | Error Handling |
|-----|-------|------------------|----------------|
| `InstanceIdentifier::Create()` | `kInvalidConfiguration` | InstanceIdentifier configuration pointer hasn't been set. | **Non-Recoverable**: Fatal configuration error - static configuration pointer must be initialized before using the class. **Fix**: Ensure `InstanceIdentifier::configuration_` is set during system initialization - this is a deployment/setup issue. |
| | `kInvalidInstanceIdentifierString` | InstanceIdentifier serialized string is invalid. | **Non-Recoverable**: Corrupted serialized data - system must terminate. <br> **Fix**: Validate JSON format before calling Create(), retry with corrected serialized string, or regenerate the instance identifier string. |
-------

### Instance Specifier APIs

#### `score::mw::com::InstanceSpecifier::Create()`

| API | Error | Error Description | Error Handling |
|-----|-------|------------------|----------------|
| `InstanceSpecifier::Create()` | `kInvalidMetaModelShortname` | Meta model short name does not adhere to naming requirements. | **Non-Recoverable**: Programming error in shortname format. <br> **Fix**: Correct shortname path to follow AUTOSAR conventions (e.g., "/ServiceName/InstanceName"). |

---
