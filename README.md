# Catalyst

<p align="center">
  <img src="docs/assets/logo.svg" alt="Catalyst logo" width="150">
</p>

> [!WARNING]
> Catalyst is under active development. Expect breaking changes and rough edges.

[Full Docs here](https://catalystcpp.github.io/catalyst-build-system/)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE.md)

Catalyst is a declarative build system for C/C++: batteries-included, but as configurable as you need.

Catalyst lets you declare toolchains, targets, dependencies, and lifecycle tasks in structured YAML manifests and then
resolves dependencies, generates configs for a backend and drives the build to completion.

## Features

- Declarative manifests: Configure targets, directories, dependencies, features, and hooks in [`catalyst.yaml`](https://catalystcpp.github.io/catalyst-build-system/concepts/configuration/) (or a unified [`CATALYST.yaml`](https://catalystcpp.github.io/catalyst-build-system/concepts/configuration/#centralized-configuration)). Targets build as `BINARY`, `STATICLIB`, `SHAREDLIB`, or `INTERFACE`.
- Profile composition: Layer configs on demand (`--profiles debug asan`); profiles cascade left to right without duplicating config. [Guide](https://catalystcpp.github.io/catalyst-build-system/concepts/profiles/).
- Extensible toolchains: Define compiler/linker/archiver commands and flags in YAML, with `extends`-based inheritance for debug/release/sanitizer variants. [Guide](https://catalystcpp.github.io/catalyst-build-system/concepts/toolchains/).
- Multi-source dependencies: Git (with revision locking), vcpkg (with transitive scanning), Conan 2.x, `pkg-config`, or local paths, all in one manifest. [Guide](https://catalystcpp.github.io/catalyst-build-system/concepts/dependencies/).
- Deterministic lockfiles: `catalyst lock` pins every dependency and writes a `catalyst.lock` so every machine resolves the same graph.
- Workspaces (monorepos): Coordinate multiple projects via [`WORKSPACE.yaml`](https://catalystcpp.github.io/catalyst-build-system/concepts/workspaces/); members auto-resolve inter-dependencies and share a lockfile.
- Feature flags & macros: Features map to C++ preprocessor macros and can gate source files. Build-info macros (`CATALYST_BUILD_SYS`, `CATALYST_PROJ_NAME`, `CATALYST_PROJ_VER`) included. [Guide](https://catalystcpp.github.io/catalyst-build-system/concepts/preprocessor/).
- Profile-aware source exclusion: [`.catalystignore`](https://catalystcpp.github.io/catalyst-build-system/concepts/catalystignore/) excludes files per profile via regex.
- Lifecycle hooks & codegen: Run commands at `pre-fetch`, `pre-build`, `post-build`, `on-build-failure`; hooks support shell, nested `catalyst` subcommands, or caching-aware codegen. [Guide](https://catalystcpp.github.io/catalyst-build-system/concepts/hooks/).
- Integrated dev workflows: `fmt` (clang-format), `tidy` (clang-tidy), `pack` (cpack), `doc` (doxygen).

## Installation

Prerequisites:
- a build executor (COB see [installation](https://catalystcpp.github.io/catalyst-orchestrated-builder/installation), `ninja`, or `make`)
- a C++ compiler (GCC/Clang/MSVC)
- whichever dependency resolvers you intend to use (`git`, `vcpkg`, `pkg-config`, Conan 2.x).
- Optional: `clang-format`, `clang-tidy`, `doxygen`, `cpack`.

Full details: [Installation Guide](https://catalystcpp.github.io/catalyst-build-system/installation/).

**Packages** — download from the [latest release](https://github.com/CatalystCPP/catalyst-build-system/releases/latest):

```bash
# Debian / Ubuntu
sudo dpkg -i catalyst_1.7.0_amd64.deb

# Fedora / RHEL / CentOS
sudo rpm -i catalyst-1.7.0-1.x86_64.rpm
```

## Quick Start

```bash
mkdir my-app && cd my-app
catalyst init    # scaffold a new binary project
catalyst build   # build it
catalyst run     # run it
```

`catalyst init` scaffolds:

```text
my-app/
├── catalyst.yaml           <-- the configuration for the common profile.
├── tc_catalyst.yaml        <-- a default catalyst toolchain configuration.
├── build/                  <-- the build output directory.
├── src/                    <-- directory for source files.
│   ├── .catalystignore     <-- a catalystignore file.
│   └── my-app.cpp          <-- a default entry point.
└── include/                <-- a default include directory.
```

with a catalyst.yaml that looks like this:


```yaml
meta:
  min_ver: 1.9.0
manifest:
  name: my-app
  type: BINARY
  version: 0.0.1
  description: Your description goes here.
  provides: ''
  toolchain: tc_catalyst.yaml
  dirs:
    include:
      - include
    source:
      - src
    build: build
```

Full walkthrough: [Getting Started Guide](https://catalystcpp.github.io/catalyst-build-system/getting_started/).

## Concepts & CLI Reference

See the [Concepts Guide](https://catalystcpp.github.io/catalyst-build-system/concepts/) for manifest, workspace, and toolchain
options. Core subcommands:

| Command | Description |
|:---|---:|
| [`init`](https://catalystcpp.github.io/catalyst-build-system/cli/init/) | Initialize a new package or profile. |
| [`add`](https://catalystcpp.github.io/catalyst-build-system/cli/add/) | Add a dependency to the project. |
| [`build`](https://catalystcpp.github.io/catalyst-build-system/cli/build/) | Build the project targets. |
| [`run`](https://catalystcpp.github.io/catalyst-build-system/cli/run/) | Execute built binaries. |
| [`test`](https://catalystcpp.github.io/catalyst-build-system/cli/test/) | Run tests mapped to profiles. |
| [`lock`](https://catalystcpp.github.io/catalyst-build-system/cli/lock/) | Generate a lockfile pinning dependency references. |

See the full [CLI Reference](https://catalystcpp.github.io/catalyst-build-system/cli/) for the rest (`clean`, `fetch`, `fmt`, `tidy`, `pack`, `doc`, and more).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup and patch guidelines.

## Release and Support Policy

Published versions are available on [GitHub Releases](https://github.com/CatalystCPP/catalyst-build-system/releases).
The following policy takes effect with Catalyst 2.0.0.

### Release Cadence

- A patch release is scheduled every week (Monday 00:00 PST). A patch may contain no functional changes.
- A minor release is scheduled every five weeks.
- A major release is scheduled every 25 weeks.
- Each week, the highest applicable release is shipped. A major release on a major-release week, a minor release on a minor-release week, and a patch release otherwise.

This naming does not necessarily conform to semver.

### Support Policy

- Every `x.0` major release line is a long-term support (LTS) line.
- An LTS line is supported until `x+3.0.0` is released, approximately 75 weeks later.
- Support applies to the latest `x.0.k` patch, not the original `x.0.0` artifact.
- Within the current major version, the latest patch of every minor line is supported until the next major version is released.
- No support is guaranteed for other releases.

## License

Apache License 2.0, see [LICENSE.md](LICENSE.md).

## Contact

Maintainer: [Siddharth Mohanty](https://www.github.com/S-Spektrum-M)

Questions or feedback? Open an [issue](https://github.com/CatalystCPP/catalyst-build-system/issues/new) or reach out
at [neosiddharth@gmail.com](mailto://neosiddharth@gmail.com).
