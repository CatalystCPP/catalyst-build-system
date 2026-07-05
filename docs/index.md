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
- [Toolchains](concepts/toolchains.md)
- [Dependencies](concepts/dependencies.md)
- [Preprocessor & Features](concepts/preprocessor.md)
- [Catalystignore](concepts/catalystignore.md)
- [C++20 Modules](concepts/modules.md)
- [Hooks](concepts/hooks.md)
- [Workspaces](concepts/workspaces.md)
- [Log](concepts/log.md)
- [Environment Variables](concepts/environment-variables.md)

### CLI Reference
- [add](cli/add.md)
- [bench](cli/bench.md)
- [build](cli/build.md)
- [clean](cli/clean.md)
- [completion](cli/completion.md)
- [doc](cli/doc.md)
- [download](cli/download.md)
- [feature-ls](cli/feature_ls.md)
- [fetch](cli/fetch.md)
- [fmt](cli/fmt.md)
- [generate](cli/generate.md)
- [ide-sync](cli/ide-sync.md)
- [init](cli/init.md)
- [install](cli/install.md)
- [lock](cli/lock.md)
- [pack](cli/pack.md)
- [profile-ls](cli/profile_ls.md)
- [run](cli/run.md)
- [test](cli/test.md)
- [tidy](cli/tidy.md)

### Community & Contributing
- [Contributing Guidelines](CONTRIBUTING.md)

---

