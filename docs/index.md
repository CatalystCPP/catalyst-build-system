# Catalyst: **A Modern, Declarative C++ Build System**

!!! warning inline end

    Catalyst is currently in early development. The command line interface and
    features are subject to change.

!!! note inline end

    This documentation refers to binaries built from the HEAD of the master
    branch of https://github.com/CatalystCPP/catalyst-build-system

Catalyst is a meta build system for C++ that aims to bring the ease of use of
declarative configuration to the C++ ecosystem. It prioritizes declarative
configuration, reproducibility, and developer experience.

## Key Features

- Declarative Configuration: Define your project with simple YAML files. No more complex CMake scripting.
- Profile Composition: Mix and match build configurations (e.g., `debug`, `release`, `asan`) easily.
- Dependency Management: Built-in support for `vcpkg`, `git` repositories, local packages, and system libraries.
- Reproducible Builds: Designed for determinism and isolation.

## Documentation

### Getting Started
- [Installation](installation.md)
- [Getting Started Guide](getting_started.md)
- [Concepts Overview](concepts/index.md)
- [Command Overview](cli/index.md)

### Core Concepts
- [Configuration](concepts/configuration.md)
- [Profiles](concepts/profiles.md)
- [Dependencies](concepts/dependencies.md)
- [Hooks](concepts/hooks.md)
- [Preprocessor & Features](concepts/preprocessor.md)
- [Catalyst Ignore](concepts/catalystignore.md)
- [Environment Variables](concepts/environment-variables.md)

### CLI Reference
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

