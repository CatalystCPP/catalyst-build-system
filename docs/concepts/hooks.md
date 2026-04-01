# Hooks

Catalyst provides a hook system that allows you to execute custom commands or scripts at various points in the build process. This enables you to extend and customize your build workflow to fit your project's specific needs.

## Configuring Hooks

You can define hooks in your `catalyst.yaml` or any profile-specific `catalyst_*.yaml` file. Hooks are defined under the `hooks` key. You can specify a single command or a list of commands to execute for each hook.

### Hook Types

Catalyst supports four types of hooks:

1. **`command`**: Executes a standard shell command (via `sh -c` or `cmd /c`).
2. **`script`**: Executes a script file (via `sh -c` or `cmd /c`).
3. **`catalyst`**: Dispatches a Catalyst subcommand internally. This is preferred for invoking other Catalyst commands (like `test` after a `build`) because it avoids shelling out to a new process, shares the current configuration context, and enforces recursion protection (depth limit of 4).
4. **`codegen`**: Executes a command for code generation with optional dependency tracking. When `input` and `output` fields are both specified, the command only runs if any output file is missing or any input file is newer than the oldest output file. This enables incremental builds by skipping generation when outputs are already up-to-date.

The `catalyst` hook type supports both a short string form and a structured map form:

**Example `catalyst.yaml`:**

```yaml
hooks:
  pre-build:
    - command: "echo 'Starting build...'"
    - catalyst: "test --help" # Short string form
  post-build:
    - command: "echo 'Build finished!'"
    - script: "scripts/notify.sh"
    - catalyst:               # Structured map form
        subcommand: clean
        args: ["--workspace"]
  on-build-failure:
    - command: "echo 'Build failed!'"

```

### Catalyst Hooks

The `catalyst` hook type dispatches a Catalyst subcommand internally without spawning a child process. This means the hook shares the current configuration context and workspace state with the parent invocation. Recursive invocation is protected by a maximum dispatch depth of 4, and direct recursion (e.g. a `pre-build` hook that triggers `build`) is always rejected.

#### Short Form

A single string parsed the same way as the CLI:

```yaml
hooks:
  post-build:
    - catalyst: "test -p release"
    - catalyst: "-V generate --backend ninja"
```

#### Structured Form

An explicit map with named fields:

```yaml
hooks:
  post-build:
    - catalyst:
        subcommand: test
        profiles: [release]
        args: ["--verbose"]
  pre-build:
    - catalyst:
        subcommand: generate
        global_args: ["-V"]
        args: ["--backend", "ninja"]
```

| Field         | Type           | Required | Description                                                |
| ------------- | -------------- | -------- | ---------------------------------------------------------- |
| `subcommand`  | `string`       | Yes      | The Catalyst subcommand to invoke (e.g., `test`).          |
| `args`        | `list[string]` | No       | Arguments passed to the subcommand.                        |
| `global_args` | `list[string]` | No       | Arguments passed before the subcommand (e.g., `-V`).       |
| `profiles`    | `list[string]` | No       | Shorthand for `-p <profile>` arguments.                    |

> [!NOTE]
> A `catalyst` hook does **not** inherit the parent invocation's global flags or profiles. Each hook invocation is independent.

### Codegen Hooks

The `codegen` hook type is designed for code generation tasks that benefit from incremental builds. It accepts the following fields:

| Field    | Required | Description                                                                 |
| -------- | -------- | --------------------------------------------------------------------------- |
| `cmd`    | Yes      | The shell command to execute.                                               |
| `input`  | No       | A file path or list of file paths that the command reads from.              |
| `output` | No       | A file path or list of file paths that the command produces.                |

When both `input` and `output` are specified, Catalyst checks file modification times to determine whether
the command needs to run. The command is skipped if all output files exist and are newer than every input file. If
either field is omitted, the command runs unconditionally on every invocation.

#### Variable Substitution

The `cmd` field supports variable substitution to reference declared inputs and outputs, avoiding path duplication:

| Variable   | Replaced With                                      |
| ---------- | -------------------------------------------------- |
| `$IN[i]`   | The input file at index `i` (zero-based).          |
| `$OUT[i]`  | The output file at index `i` (zero-based).         |
| `$IN`      | All input files, space-joined.                     |
| `$OUT`     | All output files, space-joined.                    |

An out-of-bounds index (e.g. `$IN[2]` when only two inputs are declared) is treated as an error.

**Example:**

```yaml
hooks:
  pre-generate:
    - codegen:
        cmd: "python3 tools/gen_version.py $IN[0] $OUT[0]"
        input: ["version.txt"]
        output: ["include/version.h"]
    - codegen:
        cmd: "protoc --cpp_out=src/ $IN"
        input: ["proto/messages.proto"]
        output: ["src/messages.pb.cc", "src/messages.pb.h"]
```

## Available Hooks

Here is a comprehensive list of the available hooks in Catalyst, organized by when they are triggered.

### Global Hooks

These hooks are triggered for every build and provide a way to execute commands at the very beginning and end of the build process.

| Hook              | Description                                                                                             |
| ----------------- | ------------------------------------------------------------------------------------------------------- |
| `pre-build`       | Runs once before the entire build process begins (before `generate`, `fetch`, and `build`).               |
| `post-build`      | Runs once after the entire build process has successfully completed.                                    |
| `on-build-failure`| Runs if the build fails at any stage. This is useful for logging, sending notifications, or reverting changes. |


### Subcommand Hooks

These hooks are specific to individual Catalyst subcommands, allowing you to customize the behavior of each command.

#### `generate`

| Hook           | Description                                       |
| -------------- | ------------------------------------------------- |
| `pre-generate` | Runs before the build file is generated.  |
| `post-generate`| Runs after the build file is generated.   |


#### `fetch`

| Hook         | Description                                 |
| ------------ | ------------------------------------------- |
| `pre-fetch`  | Runs before any dependencies are fetched.   |
| `post-fetch` | Runs after all dependencies have been fetched.|


#### `clean`

| Hook        | Description                              |
| ----------- | ---------------------------------------- |
| `pre-clean` | Runs before the project is cleaned.      |
| `post-clean`| Runs after the project has been cleaned. |


#### `run`

| Hook      | Description                           |
| --------- | ------------------------------------- |
| `pre-run` | Runs before the target is executed.   |
| `post-run`| Runs after the target has been executed.|

#### `test`

| Hook       | Description                                |
| ---------- | ------------------------------------------ |
| `pre-test` | Runs before the tests are executed.        |
| `post-test`| Runs after the tests have been executed.   |

#### `bench`

| Hook        | Description                                |
| ----------- | ------------------------------------------ |
| `pre-bench` | Runs before the benchmarks are executed.   |
| `post-bench`| Runs after the benchmarks have been executed.|


### Target-Specific Hooks

> [!NOTE]
> Not Yet Implemented.

These hooks can be defined within a specific build target's configuration, allowing for fine-grained control over the build process for individual targets.

| Hook        | Description                                                                 |
| ----------- | --------------------------------------------------------------------------- |
| `pre-link`  | Runs before the target is linked. This can be useful for pre-link steps.    |
| `post-link` | Runs after the target is linked. This can be useful for post-link steps.    |


### File-Specific Hooks

> [!NOTE]
> Not Yet Implemented.

These hooks are triggered when a specific file is processed, allowing for custom actions on a per-file basis.

| Hook         | Description                                                                                             |
| ------------ | ------------------------------------------------------------------------------------------------------- |
| `on-compile` | Runs when a specific source file is compiled. This can be useful for custom pre-processing or code generation. |
