# catalyst doctor

## Overview

Diagnose the current project's build readiness without fetching dependencies,
compiling, generating build files, or executing project hooks, dependency scripts,
or tool executables.

```bash
catalyst doctor
catalyst doctor -p debug asan -f logging --backend ninja
catalyst doctor --json --strict
```

## Options

| Option | Meaning |
|---|---|
| `-p, --profiles TEXT...` | Compose selected profiles, with `common` first by default, just like build. |
| `-f, --features TEXT...` | Validate feature overrides with the same resolver used by generate. |
| `--backend TEXT` | Check a backend override instead of `meta.generator`. |
| `--json` | Print a single diagnostic JSON object on stdout. |
| `--strict` | Treat warnings as failures, useful for CI preflight gates. |

`CATALYST_MACHINE` suppresses automatic `common` injection, as for build.

## Checks

Doctor reports independent problems together, with a status and an actionable hint:

- Selected profile files, YAML structure, centralized-file precedence, and shadowed split files.
- Composed artifact type and source/include directories. Missing compiled-target source
  directories are errors; missing include directories are warnings because they may be generated.
- Build destination and, on POSIX, whether its nearest existing ancestor is writable.
  Doctor does **not** create a test file or directory.
- Presence of the selected composition's `compile_commands.json`.
- Backend availability (`cob` is bundled; Ninja and Make must be installed).
- Toolchain resolution, including inheritance/cycle detection; literal executable paths
  or PATH lookup for C/C++ compilers, linker/archiver, and compiler launchers.
  Interface targets do not require compiler or linker executables.
- Feature declarations and overrides.
- Dependency declaration fields, local dependency directories/manifests, explicit
  system library/include paths, and tools required by the declared dependency sources.
- Lockfile presence at the workspace root when applicable, otherwise the current directory.

Malformed or missing selected profiles prevent composition-dependent checks, but all
selected profiles that can be examined are reported. A failed toolchain resolution does
not prevent feature or dependency diagnostics.

## Exit status and JSON

Exit status is **0** when there are no errors, **1** when there are errors, or when
`--strict` encounters any warning. Parse errors use the normal CLI exit status.

The JSON contract is versioned:

```json
{
  "schema_version": 1,
  "root": "/path/to/project",
  "profiles": ["common", "debug"],
  "strict": false,
  "healthy": true,
  "summary": {"ok": 10, "warning": 1, "error": 0},
  "checks": [
    {
      "id": "build.compile_commands",
      "status": "warning",
      "message": "No compilation database for this composition yet",
      "hint": "Run catalyst build with these profiles to generate compile_commands.json for IDE tooling."
    }
  ]
}
```

`healthy` includes the strict-mode decision. Check IDs identify categories and may
repeat (for example, one `manifest.dirs.include` check per directory). Strings are
JSON-escaped; empty hints are `""`. Ordinary logging and the final failure notice
remain on stderr; Catalyst's normal session log may still be written.

Example CI gate:

```bash
catalyst doctor -p release --json --strict > doctor-report.json
```

## Limits

This is a **preflight**, not proof that the project will build. Doctor does not run
compiler version probes, test compilation, pkg-config queries, custom dependency
commands, or project hooks. It does not recursively validate local dependencies,
verify lockfile contents, inspect installed package versions, or check build/database
freshness. Source/include directories produced by hooks may legitimately be absent;
read the hints before treating diagnostics as configuration errors.

Both configured C and C++ compilers are checked for compiled targets, regardless of
which source languages the project currently uses. Compiler launcher strings are
checked by their first whitespace-delimited executable token. Shell-style quoted
launcher paths and complex launcher expressions are not parsed.
