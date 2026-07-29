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
