# Core Concepts

Understanding the foundational concepts of Catalyst is key to effectively managing your C++ projects.

### [Catalystignore](catalystignore.md)
Exclude specific source files from the build based on active profiles using regex patterns.
This is essential for handling platform-specific or build-type-specific code without complex directory structures.

### [Configuration](configuration.md)
Define project metadata, build settings, and directory structures using intuitive YAML files.
Centralize your profile management in a single `CATALYST.yaml` for streamlined project coordination.

### [Dependencies](dependencies.md)
Manage external libraries from Git, vcpkg, local paths, or system packages with a unified interface.
Utilize lockfiles to ensure that all developers on a project are using identical dependency versions.

### [Environment Variables](environment-variables.md)
A set of internal variables that Catalyst uses to manage complex recursive dependency fetching.
These ensure that profile injection and cycle detection work correctly across nested project trees.

### [Hooks](hooks.md)
Execute custom shell commands or internal Catalyst subcommands at various stages of the build lifecycle.
Automate tasks like post-build notifications or pre-test setup directly within your profile configuration.

### [Preprocessor & Features](preprocessor.md)
Toggle code sections and conditionally compile files using dynamic feature flags defined in your config.
Catalyst automatically generates C++ macros from these features, bridging your configuration and source code.

### [Profiles](profiles.md)
Create composable build configurations that can be merged to create specific target environments.
This left-to-right merging strategy allows for powerful, modular settings for things like `linux + debug + asan`.

### [Toolchains](toolchains.md)
Configure and manage the compiler paths, flags, and build tools required for different environments.
Define custom toolchains to cross-compile or use specific compiler versions seamlessly across your projects.

### [Workspaces](workspaces.md)
Manage multiple related packages within a single repository using a centralized `WORKSPACE.yaml`.
Enable seamless cross-project dependency resolution and consolidated version locking for entire monorepos.
