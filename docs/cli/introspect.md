# catalyst introspect

```text
Query resolved state from a Catalyst lifecycle hook.

catalyst introspect [OPTIONS] [path]

POSITIONALS:
  path TEXT                   Dot-notation or indexed path into the hook state.

OPTIONS:
  -h, --help                  Print this help message and exit
  -p, --pretty                Pretty-print object and array values.
```

## Details

`introspect` reads the ephemeral state snapshot created for a running lifecycle hook.

Catalyst automatically creates and injects `CATALYST_HOOK=1` and `CATALYST_INTROSPECT_FILE` environment variables when
it spawns the hook process; users and hook scripts may chose to override the environment variables to "spoof" this
subcommand at the cost of correct behavior.

- Scalars are written as raw, unquoted text.
- `null` produces no output and succeeds.
- Objects and arrays are emitted as compact JSON unless `--pretty` is specified.
- An omitted path emits the complete state model.
- Object keys use dot notation. Sequence indices support bracket and dot syntax.

## Examples

```sh
catalyst introspect manifest.name
catalyst introspect dependencies[0].name
catalyst introspect dependencies.0.name
catalyst introspect _toolchain.compiler.cxx.flags
catalyst introspect _catalyst --pretty
```

Missing keys, out-of-bounds indices, unreadable snapshots, and invalid JSON return exit code 1 with a diagnostic on standard error.
