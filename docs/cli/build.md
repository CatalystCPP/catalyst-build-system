# catalyst build

```
Build the project.


catalyst build [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -r,     --regen [0]         Regenerate the build file.
  -b,     --force-rebuild [0]
                              Recompile dependencies.
          --force-refetch [0]
                              Refetch dependencies.
          --workspace, --all [0]
                              Build all members in the workspace.
  -w,     --watch [0]         Continuous build mode. Rebuilds on source file changes.
  -P,     --package TEXT      Build a specific package from the root.
  -p,     --profiles TEXT [[common]]  ...
                              Profile composition to build.
  -f,     --features TEXT [{}]  ...
                              Features to enable.
          --backend TEXT      Backend to use for generation (ninja, gmake, cob).
```

## Details

When running with `--workspace` or `--all`, Catalyst determines the dependency graph between workspace members.
Independent members build concurrently, while each dependent member waits for its workspace dependencies to finish
successfully.

Workspace builds do not support `--watch`; use watch mode from an individual workspace member instead.

### Feature flag overrides

`-f`/`--features` overrides the default state of feature flags declared in the `features:` block of your manifest. See the [feature flags concept](../concepts/preprocessor.md) for how flags are declared and emitted. The option accepts three forms:

| Form | Applies to | Effect |
|---|---|---|
| `-f <name>` | boolean flags | Enable the flag (define it as `1`). |
| `-f no-<name>` | boolean flags | Disable the flag (define it as `0`). |
| `-f <name>=<value>` | valued flags (`enum` / `int` / `string`) | Set the flag to `<value>`. |

You can pass `-f` multiple times to override several flags in one build:

```bash
catalyst build -f no-logging -f log_level=debug -f flush_threshold=2097152
```

Overrides are validated at configure time:

- An `enum` value must be one of the declared `values:`.
- An `int` value must be a valid integer.
- Bare `-f <name>` and `-f no-<name>` apply **only** to boolean flags. Using them on a valued flag is an error — a valued flag must be set with `<name>=<value>` (there is no "on"/"off" for a value).

Changing a flag value is picked up automatically on the next build (the value is part of each affected step's command hash); you do **not** need `-r`/`--regen` unless the flag toggles a `files:` source set.

### Build explanations (`--explain`)

The `--explain` flag enables structured diagnostic explanation logging for build planning and execution decisions.
It runs independently of `-v`/`--verbose`.

```bash
catalyst build --explain
```

#### Output Sinks

1. **Standard Error (`stderr`):** Real-time, highlighted terminal output where every explanation line is prefixed with `[EXPLAIN]`. In color-enabled terminals, the messages are rendered in bold default terminal color for visual distinction from standard log levels.
2. **Markdown Report:** A comprehensive Markdown report saved to the project directory named `catalyst_explain_[YYYY-MM-DD_HH-MM-SS].md`. If a report with the same timestamp already exists or is locked by another build, a numeric suffix is appended (e.g. `catalyst_explain_2026-10-03_18-40-00.1.md`). The report contains no ANSI escapes and formats multiline entries clearly.
3. **Structured Log Stream:** When enabled, explanation records are also recorded into `.catalyst.log` with level `EXPLAIN`.

#### Scope of explanation

The explanation report details key areas of build decision-making:
- **Invocation context:** Command-line options, working directory, official Catalyst environment variables (`CATALYST_*`), and workspace discovery status.
- **Manifest and profile composition:** Configuration discovery, profile inheritance, precedence order, sequence merges, scalar replacements, and null unsets.
- **Toolchain resolution:** Toolchain discovery, compiler executables, language standard versions, flag modifications, and flags passed to backends.
- **Features and definitions:** Feature resolution, defaults vs. CLI overrides, active feature preprocessor macros, and feature-gated source filtering.
- **Workspace planning:** Member discovery, dependency graph resolution, topological order, independent concurrent packages, and skipped packages.
- **Dependency management:** Lockfile discovery, pinned versus resolved versions, source selection (git, local, system, vcpkg, conan), and cached clone reuse.
- **Source and header discovery:** Source directory scanning, language classification, `.catalystignore` rule matching, and source-to-object mappings.
- **Hooks and code generation:** Lifecycle hooks reached, executed hook types and commands, official environment variables, `$IN`/`$OUT` substitutions, freshness comparisons, and generator execution status.
- **Build file regeneration:** Generator selection, state file comparisons, toolchain store comparisons, manifest timestamps, and the decisive condition triggering or skipping regeneration.
- **Generated build files:** Exact absolute path to the backend build file.
- **C++ modules:** Module interface/implementation categorization, scanner execution, P1689 dependency discovery, BMI paths, and compilation order.
- **Compilation database:** Absolute path of `compile_commands.json` created and published.
- **Final summary:** Overall status, built/skipped/failed packages, and report location.

#### Child session aggregation

In workspace builds or recursive dependency builds, child processes automatically forward their explanation logs to the top-level parent process via spooling (`CATALYST_EXPLAIN_SPOOL`). The parent aggregates all child records into a unified Markdown report with package-level context tagging.

#### Incompatibilities

- `--explain` cannot be used with `--watch`. Invocations combining `--explain` and `--watch` fail immediately before any build work or report creation.

#### Security & Privacy Notice

> [!WARNING]
> While Catalyst strictly limits environment reporting to official `CATALYST_*` variables, explanation reports include detailed filesystem paths, toolchain command lines, and dependency URLs. Review the generated Markdown report before sharing it publicly or attaching it to bug trackers.

## Examples

**Standard build:**
```bash
catalyst build
```

**Workspace build:**
Build all packages in the current workspace, automatically ordering them by dependency.
```bash
catalyst build --workspace
```

**Build specific package:**
Build only the `app` package and its dependencies within the workspace.
```bash
catalyst build --package app
```

**Debug build:**
```bash
catalyst build --profiles debug
```

**Force clean build:**
```bash
catalyst build --force-rebuild
```

**Enable a boolean feature:**
```bash
catalyst build --features logging
```

**Disable a boolean feature:**
```bash
catalyst build --features no-logging
```

**Set a valued feature (enum / int / string):**
```bash
catalyst build --features log_level=debug --features flush_threshold=2097152
```

**Gmake backend:**
```bash
catalyst build --backend gmake
```

**Watch mode:**
Continuously watch source and include directories for file changes and automatically rebuild.
```bash
catalyst build --watch
```

Watch mode can be combined with other flags:
```bash
catalyst build --watch --profiles debug
```

Press `Ctrl+C` to stop watching.
