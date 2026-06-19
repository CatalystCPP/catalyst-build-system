# Catalyst

> ⚠️ **Early Stage**: Catalyst is under active development. Expect breaking changes and rough edges.

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE.md)

Catalyst is a declarative, profile-aware build system for C and C++ (targeting C++20/23/26 workflows). Instead of
writing procedural build scripts, you describe your project
- targets
- dependencies
- toolchains
- and lifecycle tasks
in structured YAML manifests.

Catalyst resolves dependencies, generates configurations for a backend (COB, Ninja, or Make), and drives
the build to completion.

---

## Features

### Declarative Manifests
Configure the entire build in YAML. Targets, directories, dependencies, features, and hooks live in a
single [`catalyst.yaml`](docs/concepts/configuration.md), or in a unified
[`CATALYST.yaml`](docs/concepts/configuration.md#centralized-configuration) that holds every profile in one file.

Targets can be built as a `BINARY`, `STATICLIB`, `SHAREDLIB`, or `INTERFACE` library.

### Profile Composition
Layer build configurations on demand (e.g. `--profiles debug asan`). Profiles cascade left to right, each overlaying
flags, features, dependencies, and toolchains onto the last, so you compose targeted variants without duplicating
config. See the [Profiles Guide](docs/concepts/profiles.md).

### Extensible Toolchains
Define compiler, linker, and archiver commands and flags directly in YAML.

Toolchains support inheritance through the `extends` key, so debug, release, and sanitizer variants share a common base.
See the [Toolchains Guide](docs/concepts/toolchains.md).

### Multi-Source Dependency Management
Resolve and link external libraries from heterogeneous sources in one manifest:

- **Git** repositories, with revision locking
- **vcpkg**, with classic triplet selection and automatic transitive-dependency scanning via the status database
- **Conan 2.x** packages
- **System** libraries via `pkg-config`
- **Local** paths

See the [Dependencies Guide](docs/concepts/dependencies.md).

### Deterministic Lockfiles
`catalyst lock` pins every dependency to a precise reference (Git SHA, vcpkg triplet, etc.) and writes a
`catalyst.lock` file that takes precedence during builds so every machine resolves the same graph.

### Workspaces (monorepos)
Coordinate multiple Catalyst projects in one repository with a [`WORKSPACE.yaml`](docs/concepts/workspaces.md). Local
members resolve their inter-dependencies automatically and share a single consolidated lockfile.

### Feature flags -> preprocessor macros
Features declared in the manifest map directly to C++ preprocessor macros, and can gate which source files compile.
Catalyst also predefines build-info macros (`CATALYST_BUILD_SYS`, `CATALYST_PROJ_NAME`, `CATALYST_PROJ_VER`).
See the [Preprocessor Guide](docs/concepts/preprocessor.md).

### Profile-aware source exclusion
A [`.catalystignore`](docs/concepts/catalystignore.md) file excludes source files per profile using regex patterns,
handy for keeping platform-specific, test, or benchmark sources out of particular builds.

### Lifecycle hooks & code generation
Run commands at key build stages (`pre-fetch`, `pre-build`, `post-build`, `on-build-failure`). Hooks support raw shell execution, nested `catalyst` subcommands, or caching-aware `codegen` steps that skip regeneration when inputs and outputs are unchanged. See the [Hooks Guide](docs/concepts/hooks.md).

### Integrated developer workflows
First-class subcommands delegate to the standard toolchain: formatting (`fmt` → clang-format), static analysis (`tidy` → clang-tidy), packaging (`pack` → cpack), and API docs (`doc` → doxygen).

---

## Installation

### Prerequisites

- **Build executor**: Catalyst Orchestrated Builder (preferred see [COB installation](https://catalystcpp.github.io/catalyst-orchestrated-builder/installation)), `ninja`, or `make`.
- **C++ compiler**: GCC, Clang, MSVC, or another GNU-compatible compiler.
- **Dependency resolvers**: `git`, `vcpkg`, `pkg-config`, or Conan 2.x (as needed).
- **Developer tools** (optional): `clang-format`, `clang-tidy`, `doxygen`, `cpack`.

See the [Installation Guide](docs/installation.md) for full requirements and bootstrapping instructions.

### Packages

Download the package for your platform from the [latest GitHub release](https://github.com/CatalystCPP/catalyst-build-system/releases/latest):

```bash
# Debian / Ubuntu
sudo dpkg -i catalyst_1.7.0_amd64.deb

# Fedora / RHEL / CentOS
sudo rpm -i catalyst-1.7.0-1.x86_64.rpm
```

### Building from source

Catalyst builds itself. With an existing `catalyst` binary:

```bash
git clone https://github.com/CatalystCPP/catalyst-build-system.git
cd catalyst-build-system
catalyst build --profiles debug
```

If you're bootstrapping from scratch (no `catalyst` binary yet), follow the bootstrap steps in the [Installation Guide](docs/installation.md).

---

## Quick Start

```bash
# 1. Initialize a new binary project
mkdir my-app && cd my-app
catalyst init

# 2. Build the project
catalyst build

# 3. Run the built application
catalyst run
```

`catalyst init` scaffolds:

```text
my-app/
├── catalyst.yaml       # Project manifest
├── include/            # Public headers
└── src/                # Source files
    └── my-app.cpp      # Default entry point
```

See the [Getting Started Guide](docs/getting_started.md) for a full walkthrough.

---

## Configuration at a glance

### `catalyst.yaml` project manifest

```yaml
meta:
  min_ver: 1.7.0
  generator: cob

manifest:
  name: my-project
  type: BINARY
  version: 0.1.0
  toolchain: tc_debug.yaml
  dirs:
    include: [include]
    source: [src]
    build: build
  tooling:
    FMT: clang-format
    LINTER: clang-tidy

dependencies:
  - name: fmt
    source: git
    url: https://github.com/fmtlib/fmt.git
    version: 10.0.0
  - name: nlohmann-json
    source: vcpkg
    version: 3.11.2

features:
  logging: true
  perf_tracking:
    default: false
    files: ["src/perf.cpp"]

hooks:
  pre-build:
    - command: "echo 'Validating build configuration...'"
  post-build:
    - command: "echo 'Build complete!'"
```

### `WORKSPACE.yaml` monorepo

```yaml
my_lib:
  path: libs/my_lib
  profiles: [common, release]

my_app:
  path: apps/my_app
  profiles: [common]
```

### `tc_debug.yaml` toolchain overrides

```yaml
toolchain:
  name: gcc-debug
  compiler:
    cxx:
      executable: g++
      flags: "-std=c++23 -O0 -ggdb3 -Wall -Wextra"
  linker:
    executable: g++
    flags: "-fsanitize=address,undefined"
```

For every option, see the [Configuration Guide](docs/concepts/configuration.md).

---

## CLI Reference

Full details for each subcommand are in the [CLI Command Reference](docs/cli/index.md).

| Command | Description |
|---|---|
| [`add`](docs/cli/add.md) | Add a dependency to the project. |
| [`bench`](docs/cli/bench.md) | Execute project benchmarks. |
| [`build`](docs/cli/build.md) | Build the project targets. |
| [`clean`](docs/cli/clean.md) | Remove build artifacts. |
| [`doc`](docs/cli/doc.md) | Build package API documentation. |
| [`download`](docs/cli/download.md) | Clone, build, and install a package from a Git repo. |
| [`feature-ls`](docs/cli/feature_ls.md) | List defined features across profiles. |
| [`fetch`](docs/cli/fetch.md) | Fetch remote project dependencies. |
| [`fmt`](docs/cli/fmt.md) | Run the code formatter on source trees. |
| [`generate`](docs/cli/generate.md) | Generate build-backend configurations. |
| [`ide-sync`](docs/cli/ide-sync.md) | Sync IDE workspace metadata files. |
| [`init`](docs/cli/init.md) | Initialize a new package or profile. |
| [`install`](docs/cli/install.md) | Install build artifacts into target paths. |
| [`lock`](docs/cli/lock.md) | Generate a lockfile pinning dependency references. |
| [`pack`](docs/cli/pack.md) | Assemble packages for binary distribution. |
| [`profile-ls`](docs/cli/profile_ls.md) | List available workspace build profiles. |
| [`run`](docs/cli/run.md) | Execute built binaries. |
| [`test`](docs/cli/test.md) | Run tests mapped to profiles. |
| [`tidy`](docs/cli/tidy.md) | Perform static analysis. |

**Global options:** `-v/--version`, `-V/--verbose`, `-h/--help`.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup and patch guidelines.

## License

Apache License 2.0 see [LICENSE.md](LICENSE.md).

## Contact

**Maintainer:** [Siddharth Mohanty](https://www.linkedin.com/in/siddharth---mohanty)

Questions or feedback? Open an issue or reach out at [neosiddharth@gmail.com](mailto:neosiddharth@gmail.com).
