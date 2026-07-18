# catalyst lock

```
Resolve and pin dependencies to a catalyst.lock file.


catalyst lock [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -p,     --profiles TEXT [[common]]  ... 
                              Profile composition to use for dependency resolution.
```

## Details

The `lock` command resolves all dependencies declared in your `catalyst.yaml` (and workspace member manifests) to specific, reproducible states.

- **Git**: Resolves branches, tags, or "latest" to specific 40-character commit hashes using `git ls-remote`.
- **Vcpkg**: Resolves through manifest mode and records the actual installed
  version, port revision, triplet, and builtin registry baseline.
- **System/Local**: Records the source type and paths.

In a **Workspace**, a single `catalyst.lock` is generated at the workspace root, containing a consolidated set of pinned dependencies for all workspace members.

Running `catalyst fetch` or `catalyst build` will automatically detect and use the `catalyst.lock` file if it exists, ensuring that everyone on your team and your CI/CD pipelines use the exact same dependency versions.

## Examples

```bash
# Resolve and pin dependencies for the current project
catalyst lock

# Resolve dependencies using specific profiles
catalyst lock --profiles debug release
```
