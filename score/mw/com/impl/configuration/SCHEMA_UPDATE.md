# Extending the mw::com configuration schema

This document explains how to add or change a configuration property. For what the properties *mean*, see
[README.md](./README.md).

## Single source of truth

[mw_com_config.fbs](./mw_com_config.fbs) is the single source of truth for the configuration schema. Everything else
is derived from it:

| Artifact                                                    | Derived how                                                                 |
|-------------------------------------------------------------|-----------------------------------------------------------------------------|
| [mw_com_config_schema.json](./mw_com_config_schema.json)    | `flatc --jsonschema` + baselibs' `schema_generator` + [generate_schema.py](./generate_schema.py) (checked in) |
| `mw_com_config_generated(_patched).h`                       | `flatc --cpp` at build time (only with `--config=flatbuffers`)              |
| `<config>.bin` FlatBuffer binaries                          | [converter/json_to_flatbuffer.py](./converter/json_to_flatbuffer.py) + `flatc --binary` at build time |

Never edit `mw_com_config_schema.json` by hand. `schema_drift_test` fails if it is not byte-identical to what is
generated from the `.fbs`.

## Workflow

1. **Edit `mw_com_config.fbs`.** Follow the authoring rules in its header and the rules below.
2. **Regenerate the JSON schema** and commit it together with the `.fbs`:
   ```sh
   bazel run //score/mw/com/impl/configuration:generate_schema
   ```
   Check the `git diff` of `mw_com_config_schema.json`: it should contain exactly your change.
3. **Extend both parsing strategies.** They are independent implementations producing the same `Configuration`:
   - [configuration_json_parsing_strategy.cpp](./configuration_json_parsing_strategy.cpp) reads the public JSON
     (hyphenated keys, defaults applied in C++).
   - [configuration_flatbuffer_parsing_strategy.cpp](./configuration_flatbuffer_parsing_strategy.cpp) reads the
     generated FlatBuffer accessors (underscore names). Use `ToOptional`, `RequireValue`, `RequireNotNull` and
     `NarrowOrFatal` for optional, required and narrowed values.
4. **Add tests** (see [Tests](#tests)) and, if useful, extend [example/mw_com_config.json](./example/mw_com_config.json).
5. **Document the property** in [README.md](./README.md).
6. **Run the tests in both build configurations** (see [Build flag](#build-flag)):
   ```sh
   bazel test //score/mw/com/impl/configuration/...
   bazel test --config=flatbuffers //score/mw/com/impl/configuration/...
   ```

## Rules for the `.fbs`

### `-` vs. `_` in names

FlatBuffers identifiers cannot contain `-`, so a hyphenated public JSON key or enum value is written with `_` in the
`.fbs` (`asil-level` -> `asil_level`, `file-permissions-on-empty` -> `file_permissions_on_empty`). The tooling maps
this generically, no name is hardcoded:

- `generate_schema.py` turns **every** `_` back into `-` in property keys, `required` entries, enum values and
  enum-valued defaults.
- `json_to_flatbuffer.py` turns every `-` into `_` in JSON keys, and in string values whose underscore form is an enum
  symbol, before handing the JSON to `flatc --binary`.

Consequences:

- A public JSON key or enum value **cannot contain `_`**. It would come out with `-` in the schema.
- Keep camelCase keys exactly as they are in the JSON (`instanceSpecifier`, `eventId`, ...). Renaming one in the
  `.fbs` renames the public JSON key.
- Field names `B`/`QM` (in `AllowedConsumer`/`AllowedProvider`) are why the generated header is patched
  (`VerifyVector(B())` -> `VerifyVector(this->B())`, see the `mw_com_config_generated_h` target in [BUILD](./BUILD)).
  Avoid adding further fields with names that clash with template parameters of the generated code.

> **Outlook:** A future version 2 of the configuration format will drop hyphens and camelCase from the JSON keys.
> The keys will then match the `.fbs` field names directly, and the `-` <-> `_` conversion steps
> (`generate_schema.py` hyphen restoration, `json_to_flatbuffer.py` normalization) will no longer be needed.

### Schema metadata via doc comments

What `flatc --jsonschema` cannot derive is written as `///` tokens **before** the description lines:

```
/// @title: Service Field Getter
/// @default: false
/// @min: 0
/// @max: 255
/// @required
/// @shared: serviceVersion      (table-level: emitted once in $defs and referenced)
/// Free-form description, one or more lines.
```

`@default` is schema metadata only. If a property has a default, keep three places consistent: the `@default` token,
the FlatBuffers default (`field: bool = true;`) and the default the JSON strategy applies in C++.

### Optional and required scalars (`= null`)

A plain FlatBuffers scalar can't distinguish "absent" from "default value": an omitted key reads back as the default
(`0`, `false`, the first enum value). Therefore:

- **Optional by design** scalars, where absent and `0` mean different things (e.g. `instanceId` may be `0` or missing),
  are declared `field: T = null;`. flatc then generates a `flatbuffers::Optional<T>` accessor; convert it with
  `ToOptional`.
- **`@required`** scalars must also be declared `= null`. FlatBuffers can't mark scalars as required, so without
  `= null` a missing key would silently read back as the default. With `= null` the parser detects the missing key via
  `RequireValue` and terminates.
- Scalars with a real default (e.g. `enforceMaxSamples: bool = true;`) do **not** use `= null`.

`= null` doesn't show up in the JSON schema, because an omitted JSON key already means "absent".
`generate_json_schema` strips it before running `flatc --jsonschema`.
The optional-scalar behaviour of the FlatBuffer strategy is covered by `configuration_flatbuffer_parsing_strategy_test`
with [example/mw_com_config_optional_scalars.json](./example/mw_com_config_optional_scalars.json).

### Unknown keys

`flatc --binary` rejects JSON keys that are not in the `.fbs`. A new property must therefore be added to the `.fbs`
before any configuration that converts to a FlatBuffer can use it.

## Build flag

The FlatBuffer configuration path is experimental and gated by
`//score/mw/com/flags:experimental_enable_flatbuffers_configuration` (default `False`). Enable it with
`--config=flatbuffers` (see `.bazelrc`).

| Without the flag (default)                                                       | With `--config=flatbuffers`                                       |
|----------------------------------------------------------------------------------|-------------------------------------------------------------------|
| No FlatBuffers dependency is linked; `ConfigurationFlatbufferParsingStrategy::Parse()` logs a fatal error and terminates | `SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED` is defined and the strategy is implemented |
| `configuration_parsing_strategy_test` runs the JSON strategy only                | It runs every parametrised test for JSON **and** FlatBuffer       |
| FlatBuffer-only targets (`mw_com_config_fbs`, `configuration_flatbuffer_parsing_strategy_test`, ...) are incompatible and skipped | They are built and run |

Not gated by the flag: `generate_schema`, `schema_drift_test` and the `json_to_flatbuffer` conversion of the test
configurations. These rules only need `flatc` as a build tool.

Because a plain `bazel test` silently skips the FlatBuffer side, always run the tests **with and without**
`--config=flatbuffers` after a schema change.

## Tests

Parser test configurations live in [test/parsing_strategy_configs](./test/parsing_strategy_configs), one file per
case, and are registered in its [BUILD](./test/parsing_strategy_configs/BUILD) file in one of two lists.

### `SHARED_CONFIGS`: tested with both strategies

Every `<name>.json` is also converted to `<name>.bin` at build time via `json_to_flatbuffer`. The parametrised suites
in [configuration_parsing_strategy_test.cpp](./configuration_parsing_strategy_test.cpp)
(`ConfigurationParsingStrategyTest` & co., instantiated with `kStrategies`) parse the `.json` with the JSON strategy
and the `.bin` with the FlatBuffer strategy, then check that both produce the same result or both terminate. This is
the default: one input, two parsers, one expectation.

Use it for every configuration that flatc can convert. This includes *semantically* invalid configurations (missing
required keys, duplicates, inconsistent values), because those must be rejected by both parsers.

### `FLATC_REJECTED_CONFIGS`: tested with the JSON strategy only

Some invalid configurations can't be converted by flatc at all: unknown keys, wrong JSON types, unknown enum
symbols, numbers outside the type's range. No `.bin` can be built for them, so the FlatBuffer strategy never sees such
input. They are covered as follows:

- **JSON path:** the JSON strategy must reject them at runtime. They are tested in
  `ConfigurationJsonOnlyParsingTest` (plain `TEST_F`, no strategy parameter, using `ParseJson()`).
- **FlatBuffer path:** a `<name>_schema_rejects` test (`validate_json_schema_test` with `expected_failure = True`)
  checks that the JSON schema generated from the `.fbs` rejects the config.

The schema test stands in for "flatc rejects it" because flatc is only used through baselibs *build* rules. A
rejected input fails the build action, and Bazel can't turn a failing action into a passing test.
`rules_build_error` only supports C++ targets. A TODO in the BUILD file tracks replacing these schema tests with
build-failure tests once that is possible.

The schema is **stricter** than flatc (required keys, `@min`/`@max`). The schema rejecting a config therefore
doesn't mean flatc rejects it. Before putting a config into `FLATC_REJECTED_CONFIGS`, make sure `json_to_flatbuffer`
really fails to build it. Otherwise it belongs in `SHARED_CONFIGS`, so that the FlatBuffer strategy's runtime
checks are tested too.

### Adding a test

1. Add `test/parsing_strategy_configs/<name>.json`.
2. Add `<name>` to `SHARED_CONFIGS` or, only if flatc can't convert it, to `FLATC_REJECTED_CONFIGS`.
3. Add a `TEST_P(ConfigurationParsingStrategyTest, ...)` using `Parse(GetParam(), "<name>")`, or for
   flatc-rejected configs a `TEST_F(ConfigurationJsonOnlyParsingTest, ...)` using `ParseJson("<name>")`.
