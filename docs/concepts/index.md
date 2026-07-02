# Core Concepts

Understanding the foundational concepts of Catalyst is key to effectively managing your C++ projects.

The concepts below are ordered as a learning path so that each one builds on the ones before it.

## Foundations

### [Configuration](configuration.md)

Define project metadata, build settings, and directory structures using YAML files.
Centralize your profile management in a single `CATALYST.yaml` for streamlined project coordination.

### [Profiles](profiles.md)

Create composable configurations that can be merged to create specific target environments.

This left-to-right merging strategy allows for modular settings for things like `linux + debug + asan`.

### [Toolchains](toolchains.md)

Configure and manage the compiler paths, flags, and build tools required for different environments.
Define custom toolchains to cross-compile or use specific compiler versions seamlessly across your projects.

## Building Projects

### [Dependencies](dependencies.md)

Manage external libraries from Git, vcpkg, conan, local paths, or system packages with a unified interface.

Utilize lockfiles to ensure that all developers on a project are using identical dependency versions.

### [Preprocessor & Features](preprocessor.md)

Toggle code sections, select among enumerated or numeric values, and conditionally compile files using feature flags
defined in your config.
Catalyst automatically generates preprocessor macros from these features, bridging your configuration and source code.

### [Catalystignore](catalystignore.md)

Exclude specific source files from the build based on active profiles using regex patterns.
This is essential for handling platform-specific or build-type-specific code without complex directory structures.

## Lifecycle

### [Hooks](hooks.md)

Execute custom shell commands or internal Catalyst subcommands at various stages of the build lifecycle.
Automate tasks like post-build notifications or pre-test setup directly within your profile configuration.

### [Workspaces](workspaces.md)

Manage multiple related packages within a single repository using a centralized `WORKSPACE.yaml`.
Enable cross-project dependency resolution and consolidated version locking for entire monorepos.

## Reference

### [Log](log.md)

Capture both structured data for automated analysis and colored terminal output for developers.
Understand how session events and standard entries are recorded in the console and `.catalyst.log` file.

### [Environment Variables](environment-variables.md)

A set of internal variables that Catalyst uses to manage complex recursive dependency fetching.
These ensure that profile injection and cycle detection work correctly across nested project trees.
