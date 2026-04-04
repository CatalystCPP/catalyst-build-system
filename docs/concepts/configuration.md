# Configuration

Catalyst projects are configured using YAML files. The primary configuration file
is `catalyst.yaml`, which defines the `common` profile. Additional profiles can be defined
in `catalyst_<profile>.yaml` files.

## Schema Overview

A Catalyst profile configuration consists of several top-level sections:

```yaml
meta:        # Catalyst-specific metadata
manifest:    # Project identity and structure
dependencies:# External packages
features:    # Feature flags
hooks:       # Lifecycle scripts
```

---

## `meta`

Defines constraints for the Catalyst tool itself.

| Field | Type | Default | Description |
|---|---|---|---|
| `min_ver` | String | `0.0.0` | Minimum Catalyst version required to build the project. |
| `generator` | String | `cbe` | The generator to use. Supported: `cbe` (default/recommended), `ninja`, `gmake` (or `make`). |

---

## `manifest`
Defines the project's identity, build settings, and directory structure.

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | String | (Current Dir) | Project name. Must be unique in a workspace. |
| `type` | String | `BINARY` | Artifact type: `BINARY`, `STATICLIB`, `SHAREDLIB`, `INTERFACE`. |
| `version` | String | `0.0.1` | Project version string. |
| `description`| String | ... | Human-readable description. |
| `author` | String | "" | Project author. Used by the `pack` subcommand. |
| `maintainer` | String | "" | Project maintainer. Used by the `pack` subcommand. |
| `vendor` | String | "Catalyst" | Project vendor. Used by the `pack` subcommand. |
| `license_file` | String | "" | Path to the license file. Used by the `pack` subcommand. |
| `readme_file` | String | "" | Path to the readme file. Used by the `pack` subcommand. |
| `provides` | String | - | Output artifact name pattern (e.g., `*.so`). |
| `tooling` | Object | - | Compiler toolchain overrides. |
| `dirs` | Object | - | Source and build directory configuration. |

> **Note**: Fields like `author`, `maintainer`, `vendor`, `license_file`, and `readme_file` are currently only utilized by the `catalyst pack` subcommand to generate package metadata.

### `manifest.tooling`

| Field | Description | Default |
|---|---|---|
| `CC` | C Compiler | `clang` |
| `CXX` | C++ Compiler | `clang++` |
| `CC_LAUNCHER` | C Compiler Launcher (e.g., ccache) | "" |
| `CXX_LAUNCHER`| C++ Compiler Launcher (e.g., ccache) | "" |
| `CCFLAGS` | C Compiler Flags | "" |
| `CXXFLAGS` | C++ Compiler Flags | "" |
| `LDFLAGS` | Linker Flags | "" |

### `manifest.dirs`

| Field | Description | Default |
|---|---|---|
| `include` | List of include directories | `[include]` |
| `source` | List of source directories | `[src]` |
| `build` | Output directory | `build` |

> **Note**: `dirs.source` is recursive. Use `.catalystignore` to exclude files.

---

## `dependencies`

A list of dependencies. See [Dependencies](dependencies.md) for detailed schema.

```yaml
dependencies:
    - name: fmt
      source: git
      url: https://github.com/fmtlib/fmt.git
      version: 8.1.1
```

---

## `features`

Defines boolean feature flags that can be toggled via profiles or CLI.
Enabling a feature defines a preprocessor macro.
Features can optionally list source files that will be compiled if and only if the feature is enabled.

```yaml
features:
  logging: true
  work_estimates:
    default: true
    files: ["src/work_estimate.cpp"]
```

See [Preprocessor & Features](preprocessor.md) for details.

---

## `hooks`

Defines scripts to run at specific build lifecycle stages.

```yaml
hooks:
  pre-build:
    - command: "echo 'Starting build...'"
```

See [Hooks](hooks.md) for the full list of available hooks.

## Centralized Configuration

For projects that will be defining multiple profiles, having multiple `catalyst_*.yaml` profile files is cumbersome to
manage and pollutes the top level of the directory. Instead, these projects can opt to use a `CATALYST.yaml` for
centralized configuration as such (for brevity, actual contents are omitted):

```yaml
common:
    # what one would expect in a catalyst.yaml
debug:
    # what one would expect in a catalyst_debug.yaml
release:
    # what one would expect in a catalyst_release.yaml
test:
    # what one would expect in a catalyst_test.yaml
```
