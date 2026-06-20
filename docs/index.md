# Catalyst: **A Modern, Declarative C++ Build System**

> WARNING: Catalyst is currently in early development. APIs and features are subject to change.
> NOTE: This documentation refers to binaries built from the 1.1.0 tag.

Catalyst is a next-generation build system for C++ that aims to bring the ease of use of declarative configuration to
the C++ ecosystem. It prioritizes declarative configuration, reproducibility, and developer experience.

## Key Features

- **Declarative Configuration**: Define your project with simple YAML files. No more complex CMake scripting.
- **Profile Composition**: Mix and match build configurations (e.g., `debug`, `release`, `asan`) easily.
- **Dependency Management**: Built-in support for `vcpkg`, `git` repositories, local packages, and system libraries.
- **Reproducible Builds**: Designed for determinism and isolation.
- **Modern C++ Support**: Built for C++20 and beyond.

## Documentation

### Getting Started
- [Installation](installation.md)
- [Getting Started Guide](getting_started.md)

### Core Concepts
- [Configuration](concepts/configuration.md)
- [Profiles](concepts/profiles.md)
- [Dependencies](concepts/dependencies.md)
- [Hooks](concepts/hooks.md)
- [Preprocessor & Features](concepts/preprocessor.md)
- [catalystignore](concepts/catalystignore.md)
- [Environment Variables](concepts/environment-variables.md)

### CLI Reference
- [Command Overview](cli/index.md)
- [init](cli/init.md)
- [add](cli/add.md)
- [build](cli/build.md)
- [run](cli/run.md)
- [install](cli/install.md)
- [test](cli/test.md)
- [fetch](cli/fetch.md)
- [generate](cli/generate.md)
- [clean](cli/clean.md)
- [fmt](cli/fmt.md)
- [tidy](cli/tidy.md)
- [download](cli/download.md)

### Community & Contributing
- [Contributing Guidelines](CONTRIBUTING.md)

---

