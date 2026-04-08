# CLI Command Reference

Catalyst provides a suite of subcommands to manage the entire project lifecycle.

| Command | Description |
|---|---|
| [`add`](add.md) | Add a dependency to the project. |
| [`bench`](bench.md) | Execute project benchmarks. |
| [`build`](build.md) | Build the project. |
| [`clean`](clean.md) | Remove build artifacts. |
| [`doc`](doc.md) | Build a package's documentation. |
| [`download`](download.md) | Download, build, and install a project from git. |
| [`feature-ls`](feature_ls.md) | List all available features across all profiles. |
| [`fetch`](fetch.md) | Fetch remote dependencies. |
| [`fmt`](fmt.md) | Format source code. |
| [`generate`](generate.md) | Generate build scripts (Ninja, Make, etc.). |
| [`ide-sync`](ide-sync.md) | Sync IDE configuration files for an existing project. |
| [`init`](init.md) | Initialize a new project or profile. |
| [`install`](install.md) | Install build artifacts. |
| [`lock`](lock.md) | Pin dependency versions to a lockfile. |
| [`pack`](pack.md) | Assemble the local package for distribution. |
| [`profile-ls`](profile_ls.md) | List all available profiles in the current workspace. |
| [`run`](run.md) | Run the built executable. |
| [`test`](test.md) | Run project tests. |
| [`tidy`](tidy.md) | Run static analysis. |

## Global Options

These options apply to all commands:

- `-v, --version`: Print version information.
- `-V, --verbose`: Enable verbose logging (debug output).
- `-h, --help`: Print help message.
