#  Road Map

| Subcommand | Status | Description |
|------------|--------|-------------|
| `pack` | Implemented | Assemble the local package for distribution. |
| `bench` | Implemented | Execute all benchmarks of a local package. |
| `doc` | Implemented | Build a package's documentation. |
| `generate-lockfile` | Implement (as catalyst lock) | Generate the lockfile for a package. |
| `metadata` | Planned | Output the resolved dependecies of a package, the concrete used versions including overrides, in machine-readable format miri. |
| `owner` | Planned | Manage the owners of a package on the registry. |
| `publish` | Planned | Uplaot a package to the registry. |
| `remove` | Planned | Remove dependencies from a Cargo.toml manifest file. |
| `tree` | Planned | Display a tree visualization of a dependency graph. |
| `report` | Out of Scope | Generate and display various kinds of reports |
| `update` | Out of Scope | Update dependencies as recorded in the local lock file |
| `vendor` | Out of Scope | Vendor all dependencies for a project locally |
| `version` | Out of Scope | Show version information |
| `yank` | Out of Scope | Remove a pushed package from the index |
| `help` | Out of Scope | Displays help for a catalyst subcommand |
| `info` | Out of Scope | Display information about a package |
| `locate-project` | Out of Scope | Print a JSON representation of a Cargo.toml file's location |
| `login` | Out of Scope | Log in to a registry. |
| `logout` | Out of Scope | Remove an API token from the registry locally |
| `install` | Out of Scope | Install a binary |
| `uninstall` | Out of Scope | Remove a binary |
| `pkgid` | Out of Scope | Print a fully qualified package specification |
| `search` | Out of Scope | Search packages in the registry. Default registry is TBD. |
