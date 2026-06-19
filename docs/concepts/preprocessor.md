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

> [!NOTE]
> Read [build subcommand](../cli/build.md) docs for info on how to dynamically enable or disable features.

Features defined in `catalyst.yaml` map directly to preprocessor macros. You can also specify an optional list of source files to compile only when the feature is enabled.

**Configuration:**
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

**Generated Macros:**
- `FF_my_app__logging` (Defined as `1`)
- `FF_my_app__gui` (Defined as `0`)
- `FF_my_app__work_estimates` (Defined as `1`)

**Usage in Code:**
```cpp
#if FF_my_app__logging
    log("This is logged.");
#endif
```

## Custom Flags

You can define arbitrary macros via the compiler flags in your [toolchain](toolchains.md) file:

```yaml
# tc_my-project.yaml
toolchain:
  compiler:
    cxx:
      flags: "-DENABLE_EXPERIMENTAL"
```
