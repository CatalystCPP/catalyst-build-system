# Catalyst

> ⚠️ **Early Stage**: Catalyst is under active development. Expect breaking changes and rough edges.

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE.md)

---

## What is Catalyst?

Catalyst is a declarative build system that brings Cargo-like simplicity to C and C++. No more wrestling with CMake scripts or Makefiles—just clean YAML configuration and reproducible builds.

**Why Catalyst?**

- Declarative – YAML config files instead of scripting languages.
- Profile-based builds – Compose configurations like `debug + asan` or `release + lto`.
- Built-in dependency management – Native support for vcpkg, git repos, local packages, and system libraries.
- Reproducible by default – Per-target isolation with deterministic builds.
- Modern C++ focused – Designed for C++20/23/26 workflows.

---

## Documentation

Documentation is now in the [`docs/`](docs/) directory and is hosted
[here](https://catalystcpp.github.io/catalyst-build-system/).

### 🚀 Getting Started

- [Installation](docs/installation.md): How to build and install Catalyst.
- [Getting Started Guide](docs/getting_started.md): Create your first "Hello World" project.
- [CLI Reference](docs/cli/index.md): Usage guide for `catalyst build`, `catalyst run`, etc.

### 📘 Core Concepts

- [Configuration](docs/concepts/configuration.md): The full schema for `catalyst.yaml`.
- [Profiles](docs/concepts/profiles.md): How to use and compose build profiles (e.g., Debug, Release).
- [Dependencies](docs/concepts/dependencies.md): Managing external libraries (Git, Vcpkg, System).
- [Hooks](docs/concepts/hooks.md): Lifecycle callbacks for custom scripts.
- [Catalyst Ignore](docs/concepts/catalystignore.md): Excluding files from builds based on profiles.

---

## Quick Look

### `catalyst.yaml` File

Catalyst uses a simple YAML file to define your project.

```yaml
manifest:
  name: my-project
  type: BINARY
  version: 0.1.0
  tooling:
    CXX: clang++
    CXXFLAGS: -std=c++20 -Wall -Wextra
  dirs:
    include: [include]
    source: [src]
    build: build

dependencies:
  - name: fmt
    source: git
    url: https://github.com/fmtlib/fmt.git
    version: 10.0.0
```

### Core Commands

| Command | Description |
|---------|-------------|
| `catalyst init` | Create a new project |
| `catalyst build` | Build the project |
| `catalyst run` | Run the binary |
| `catalyst lock` | Pin dependencies |
| `catalyst add` | Add a dependency |

For a full list, see the **[CLI Reference](docs/cli/index.md)**.

---

## Roadmap & Contributing

- **[Roadmap](ROADMAP.md)**: See what features are planned (e.g., Workspaces, Windows support).
- **[Contributing](CONTRIBUTING.md)**: Want to help? Read our contribution guidelines.

---

## License

Apache 2.0 – See [LICENSE.md](LICENSE.md)

---

## Contact

**Maintainer:** [Siddharth Mohanty](https://www.linkedin.com/in/siddharth---mohanty)

Questions or feedback? Open an issue or reach out at [neosiddharth@gmail.com](mailto:neosiddharth@gmail.com).
