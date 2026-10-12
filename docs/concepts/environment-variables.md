# Environment Variables

Catalyst reads and sets a small number of environment variables at runtime. Most are internal and managed automatically; they are documented here for transparency and advanced use.

## Hook context

Catalyst automatically creates and injects the following variables into every `command`, `script`, and `codegen` hook process:

| Variable | Description |
|---|---|
| `CATALYST_HOOK` | Always `1`; authorizes the guarded `catalyst introspect` command. |
| `CATALYST_HOOK_NAME` | Active lifecycle phase, such as `pre-build` or `post-test`. |
| `CATALYST_WORKSPACE_ROOT` | Absolute project or workspace root. |
| `CATALYST_PROFILES` | Comma-separated active profile composition. |
| `CATALYST_FEATURES` | Comma-separated enabled feature names. |
| `CATALYST_BUILD_DIR` | Resolved multiplexed build directory. |
| `CATALYST_INTROSPECT_FILE` | Ephemeral read-only JSON state snapshot used by `catalyst introspect`. |

These values are scoped to the child process. The snapshot is synchronized before each phase and removed automatically when the owning Catalyst configuration is destroyed.

!!! warning

    `CATALYST_HOOK` and `CATALYST_INTROSPECT_FILE` are internal authorization and transport details. Manual override may break behavior.

---

## `CATALYST_MACHINE`

Set by Catalyst when it spawns any child Catalyst process (local dependency fetches and builds, workspace member builds).
When present, Catalyst skips automatically prepending the `common` profile to the invocation.

This prevents the `common` profile from being injected multiple times as dependency fetches recurse into nested projects.

!!! warning

    This variable is intended for internal use. Setting it manually will suppress `common` profile injection for that invocation.

---

## `CATALYST_VERBOSE`

Set by Catalyst when it spawns a child Catalyst process to build a local dependency
while verbose logging (`-V`) is enabled. When present, the child process enables verbose logging
automatically, even without the `-V` flag on its command line.

This ensures that debug-level log output propagates through the entire dependency build chain.

!!! warning

    This variable is intended for internal use. Setting it manually will enable verbose logging for that invocation.

---

## `CATALYST_EXPLAIN_SPOOL`

Set by Catalyst when running an explanation session (`catalyst build --explain`) and spawning a child Catalyst process (such as a workspace member build or a recursive local dependency build).

The variable contains the path to an ephemeral spool file (`.part`). When present:

- The child process recognizes that an active explanation session is already owned by its parent.
- Instead of creating an independent `catalyst_explain_[timestamp].md` report, the child streams its explanation records directly into the spool file.
- The parent process collects the spooled records upon child completion, merges them into the top-level explanation report with package-level context tagging, and deletes the temporary spool file.

Catalyst also passes `CATALYST_EXPLAIN_ROLE` alongside `CATALYST_EXPLAIN_SPOOL` (see [`CATALYST_EXPLAIN_ROLE`](#catalyst_explain_role)).

!!! warning

    This variable is an internal transport mechanism for explanation aggregation. Setting it manually will divert explanation output into the specified file path instead of creating a standard report.

---

## `CATALYST_EXPLAIN_ROLE`

Set by Catalyst alongside `CATALYST_EXPLAIN_SPOOL` during an explanation session (`catalyst build --explain`).
It holds the child build's label in the aggregated explanation report: the workspace member's package name, or the dependency name for a local dependency build.

!!! warning

    This variable is intended for internal use. Setting it manually only changes the label used in the aggregated explanation report.

---

## `CATALYST_VISITED`

Set by Catalyst when it recursively builds a local dependency. It is a colon-separated list of the local project paths already in the current dependency chain, and it is used for cycle detection.

Before building a local dependency, Catalyst compares its resolved path against each entry. A match (by filesystem equivalence or exact string) fails the fetch with `Dependency cycle detected involving <path>`. Otherwise the child receives the list extended with the new path.

!!! warning

    This variable is intended for internal use. Setting it manually can cause spurious cycle errors or disable cycle detection for that invocation.

---

## `CATALYST_LOG_PATH`

Set by Catalyst to the absolute path of the active log generation file for the current invocation.

A child Catalyst process started with `CATALYST_MACHINE` reads this variable and appends to the parent's log generation instead of rotating logs or writing to its own `.catalyst.log`. This works even when the child runs from a different working directory or while another top-level build is running. See [Log](log.md) for details on log generations.

!!! warning

    This variable is intended for internal use. Setting it manually redirects a machine-invoked process's log output to the given file.

---

## `CATALYST_DEP_NAME` and `CATALYST_DEP_VERSION`

Set by Catalyst when it runs the `command` or `script` of a [`custom`](dependencies.md#6-custom) dependency source.

- `CATALYST_DEP_NAME` is the dependency's `name`, or `<unnamed>` if it has none.
- `CATALYST_DEP_VERSION` is the dependency's `version`. It is only set when the dependency declares one.

The command or script must write its `-I`, `-L`, and `-l` flags to stdout. These variables are scoped to that process.

