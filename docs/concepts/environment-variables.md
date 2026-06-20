# Environment Variables

Catalyst reads and sets a small number of environment variables at runtime. Most are internal and managed automatically; they are documented here for transparency and advanced use.

## `CATALYST_MACHINE`

Set by Catalyst when it spawns a child Catalyst process to fetch a local dependency.
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
