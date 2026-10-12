# Profiles

Profiles are a core concept in Catalyst, allowing you to define different build configurations
(e.g., Debug, Release, Testing) and compose them together.

## Defining Profiles

- **`catalyst.yaml`**: Defines the base `common` profile.
- **`catalyst_<name>.yaml`**: Defines a profile named `<name>`.

### Example

**`catalyst.yaml` (common)**
```yaml
manifest:
  name: my-app
  version: 1.0.0
```

**`catalyst_debug.yaml` (debug)**
```yaml
manifest:
  # Compiler/linker flags live in a toolchain file (see Toolchains).
  toolchain: tc_debug.yaml
```

**`catalyst_release.yaml` (release)**
```yaml
manifest:
  toolchain: tc_release.yaml
```

## Composition

Catalyst allows you to compose profiles by listing them. Settings are merged from left to right, with later profiles
overriding earlier ones for scalar values (see [Merge rules](#merge-rules)).

```bash
catalyst build --profiles debug release
```

In this example:
1.  Start with `common`.
2.  Merge `debug` settings (overriding `common` where conflicts occur) yielding `common-debug`
3.  Merge `release` settings, overriding where conflicts occur, and yielding `common-debug-release`.

This allows for combinations like `linux + debug + asan`.

### Merge rules

Scalars are replaced by later profiles, but not every key merges the same way:

| Key | Rule |
|-----|------|
| scalars (`manifest.name`, `type`, `version`, `toolchain`, `dirs.build`, `tooling.*`, `meta.generator`) | later profile replaces |
| `meta.min_ver` | highest version wins |
| `meta.generator: null` | falls back to `cob` |
| `manifest.dirs.include` / `dirs.source` | appended in order; duplicates kept |
| `dependencies` | appended; an entry with the same `name` replaces the earlier one entirely (no field merge), and the replacement moves to the end of the list; `dependencies: null` clears the list |
| `features` | merged per flag name; a later definition replaces that flag; a flag set to `null` is kept but treated as off |
| `hooks` | each phase's actions are appended; a phase set to `null` is removed |
| any other key set to `null` | removes the inherited value |
| adjacent duplicate profiles (e.g. `debug debug`) | the repeat is ignored with a warning |

## Nulling Fields

To unset a value inherited from a previous profile, set it to `null`.

```yaml
# catalyst_custom.yaml
meta: null  # Removes 'meta' section entirely
```

## Reserved Profiles

- **`common`**: The base profile (`catalyst.yaml`).
- **`test`**: Default test profile used by `catalyst test` (`catalyst_test.yaml`). Composed by default as `common test`, but can be customized or overridden via `catalyst test -p, --profiles <profiles...>`.
- **`bench`**: Automatically used by `catalyst bench` (`catalyst_bench.yaml`).

## Centralized Definition

For projects defining multiple profiles, managing multiple `catalyst_*.yaml` files can become cumbersome.
Catalyst supports a centralized `CATALYST.yaml` file where all profiles can be defined in a single place.

When `CATALYST.yaml` is present, Catalyst will prioritize loading profiles from it. If a profile is not found in
`CATALYST.yaml`, it will fall back to looking for the corresponding `catalyst_<profile>.yaml` file.

**Example `CATALYST.yaml`:**
```yaml
common:
  manifest:
    name: my-app
    version: 1.0.0

debug:
  manifest:
    toolchain: tc_debug.yaml

release:
  manifest:
    toolchain: tc_release.yaml

test:
  manifest:
    name: my-app-tests
```
