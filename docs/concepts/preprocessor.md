# Preprocessor Variables & Features

Catalyst integrates tightly with the C++ preprocessor to pass build information and feature flags into your code.

## Catalyst-Defined Macros

These macros are automatically defined by Catalyst during compilation.

| Macro | Description |
|---|---|
| `CATALYST_BUILD_SYS` | Always defined. Indicates the code is being built by Catalyst. |
| `CATALYST_PROJ_NAME` | The project name (from `manifest.name`). |
| `CATALYST_PROJ_VER` | The project version (from `manifest.version`). |

## Feature Flags

!!! tip

    Read the [build subcommand docs](../cli/build.md) for how to override feature flags from
    the command line (`-f`/`--features`).

Features defined in the `features:` block of your manifest map directly to preprocessor macros named
`FF_<project>__<feature>`, where `<project>` is `manifest.name`.

Every declared flag is always defined: a disabled boolean is emitted as `0`, not left undefined.
This means a typo in `#if FF_my_app__loging` is a hard always-false rather than a silent one,
and compiling with `-Wundef` will flag the misspelling.

### Boolean Flags

The simplest flag is an on/off boolean. Write it as a bare value, or as a map with a `default:` and an optional
list of source `files:` that are compiled only when the flag is enabled.

```yaml
manifest:
  name: my_app
features:
  logging: true
  gui: false
  predictive_execution:
    default: true
    files: ["src/predictive_execution.cpp"]
```

**Generated macros:**
- `FF_my_app__logging` → `1`
- `FF_my_app__gui` → `0`
- `FF_my_app__predictive_execution` → `1`

```cpp
#if FF_my_app__logging
    log("This is logged.");
#endif
```

!!! tip

    Toggling a `files:` flag changes which sources are in the build graph, so it requires a regenerate
    (`catalyst build -r`). Toggling a flag that only changes a define does not require a regenerate; the value is
    part of each affected step's command hash and is picked up automatically.

### Valued Flags (`enum` / `int` / `string`)

Not every flag is naturally a boolean. A flag can carry a value by adding a `type:`.
Valued flags require an explicit `default:`.

```yaml
manifest:
  name: cob
features:
  log_level:
    type: enum
    values: [off, error, info, debug]   # ordered; the index is the emitted int
    default: error
  flush_threshold:
    type: int
    default: 1048576                     # 1 MiB
  build_id:
    type: string
    default: "dev-build"                 # runtime-only; cannot drive #if
```

**`enum`** — the selected member is emitted as its **index** into `values:`. One named-constant macro is also
emitted per member so comparisons read well:

```c
#define FF_cob__log_level        1   // = error (the active value)
#define FF_cob__log_level__off   0
#define FF_cob__log_level__error 1
#define FF_cob__log_level__info  2
#define FF_cob__log_level__debug 3
```
```cpp
#if FF_cob__log_level >= FF_cob__log_level__info
    // verbose logging compiled in
#endif
```

**`int`** — emitted as the literal value, usable directly in `#if` and as a `constexpr`:

```c
#define FF_cob__flush_threshold 1048576
```

**`string`** — emitted as a quoted string literal. **A string flag cannot drive `#if`** (the preprocessor cannot compare strings); it is meant for runtime use such as version strings or paths:

```c
#define FF_cob__build_id "dev-build"
```

**Validation** (reported at configure time):

- `enum` requires a non-empty `values:` list, and both the `default:` and any CLI override must be one of those values.
- `int` requires the `default:` (and any CLI override) to be a valid integer.
- Every valued flag requires a `default:`.

A bare `feature: true|false` remains valid and behaves as before — it is sugar for an on/off flag.

### Overriding from the CLI

Defaults can be overridden per build with `-f`/`--features`.

Booleans use `-f <name>` / `-f no-<name>`; valued flags use `-f <name>=<value>`:

```bash
catalyst build -f no-logging -f log_level=debug -f flush_threshold=2097152
```

See the [build subcommand](../cli/build.md#feature-flag-overrides) for the full grammar and validation rules.

## Custom Flags

You can always define arbitrary macros via the compiler flags in your [toolchain](toolchains.md) file:

```yaml
# tc_my-project.yaml
toolchain:
  compiler:
    cxx:
      flags: "-DENABLE_EXPERIMENTAL"
```
