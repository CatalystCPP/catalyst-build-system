# Environment Variables

Catalyst reads and sets a small number of environment variables at runtime. Most are internal and managed automatically; they are documented here for transparency and advanced use.

## `CATALYST_MACHINE`

**Type:** Flag (any non-empty value is truthy)

Set by Catalyst when it spawns a child Catalyst process to fetch a local dependency. When present, Catalyst skips automatically prepending the `common` profile to the invocation.

This prevents the `common` profile from being injected multiple times as dependency fetches recurse into nested projects.

> **Note:** This variable is intended for internal use. Setting it manually will suppress `common` profile injection for that invocation.

---

## `CATALYST_VISITED`

**Type:** Colon-separated list of absolute paths

Tracks which local dependency paths have already been visited during a recursive `fetch` operation. Set and propagated by Catalyst across child processes to detect and break dependency cycles.

> **Note:** This variable is intended for internal use. Do not set it manually.

## CATALYST_VERBOSE

Type; Flag (any non-empty value is truthy)

Set by Catalsyt when it's invoked with -V AND spawns a child Catalyst to fetch a local dependency.
When present, the child catalyst is also made verbose.
