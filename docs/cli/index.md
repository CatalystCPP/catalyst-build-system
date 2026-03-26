# CLI Command Reference

Catalyst provides a suite of subcommands to manage the entire project lifecycle.

| Command | Description |
|---|---|
| [`init`](init.md) | Initialize a new project or profile. |
| [`add`](add.md) | Add a dependency to the project. |
| [`build`](build.md) | Build the project. |
| [`run`](run.md) | Run the built executable. |
| [`test`](test.md) | Run project tests. |
| [`bench`](bench.md) | Execute project benchmarks. |
| [`fetch`](fetch.md) | Fetch remote dependencies. |
| [`generate`](generate.md) | Generate build scripts (Ninja, Make, etc.). |
| [`install`](install.md) | Install build artifacts. |
| [`lock`](lock.md) | Pin dependency versions to a lockfile. |
| [`clean`](clean.md) | Remove build artifacts. |
| [`download`](download.md) | Download, build, and install a project from git. |
| [`fmt`](fmt.md) | Format source code. |
| [`tidy`](tidy.md) | Run static analysis. |
| [`pack`](pack.md) | Assemble the local package for distribution. |
| [`doc`](doc.md) | Build a package's documentation. |
| [`profile-ls`](profile_ls.md) | List all available profiles in the current workspace. |

## Global Options

These options apply to all commands:

- `-v, --version`: Print version information.
- `-V, --verbose`: Enable verbose logging (debug output).
- `-h, --help`: Print help message.
