# Installation

## Prerequisites

### Build Executor

Based on your build backend, catalyst will want one of the following:

- Catalyst Build Executor (`cbe`) (preferred)
    - Follow the [instructions](https://catalystcpp.github.io/catalyst-build-executor/installation).
- `ninja`
    - Available on most package managers
- or `make`
    - Available on most package managers


### C++ Compiler

Out of the box Catalyst works with any gnu-compatible compiler, namely `gcc`, `g++`, `clang`, and `clang++`.

Catalyst can be made to work with other compilers via [Toolchain Definitions](concepts/toolchains.md). A standard Windows
toolchain definition is made available.

### Dependency Managers

- Catalyst depends on the following programs for dependency management
    - `git`
        - Available on most package managers.
    - `vcpkg`
        - Follow the [Windows](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-powershell) or [UNIX](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-bash) instructions.
    - `pkg-config`
        - Available on most package managers.

### Optional Tools

- Catalyst relies on external tools for certain subcommands:
    - `doxygen` (or `clang-doc` / `jocasta`)
        - Required for the `catalyst doc` subcommand. Available on most package managers.
    - `clang-format`
        - Required for the `catalyst fmt` subcommand. Available on most package managers.
    - `clang-tidy`
        - Required for the `catalyst tidy` subcommand. Available on most package managers.
    - `cpack` (distributed with `cmake`)
        - Required for the `catalyst pack` subcommand. Available on most package managers.

## Installation

After fetching all prerequisites, download and install the relevant package from the [latest GitHub release](https://github.com/CatalystCPP/catalyst-build-system/releases/latest). Currently, only `.deb` and `.rpm` packages are provided.

For example, to install:

**Debian/Ubuntu (`.deb`):**
```bash
sudo dpkg -i catalyst_1.1.0_amd64.deb
```

**Fedora/RHEL (`.rpm`):**
```bash
sudo rpm -i catalyst-1.1.0-1.x86_64.rpm
```

- TIP: If an install fails, please open an issue with the error message
- TIP: If your preferred format is not available, please open an issue requesting that it be provided.
- NOTE: the package names might be different based on the version you are installing.

### Building from Source

1.  **Clone the repository:**

```bash
git clone https://github.com/CatalystCPP/catalyst-build-system.git
cd catalyst
```

2.  **Build the Bootstrap version using CMake:**

```bash
git checkout tags/1.0.0
cmake -B build-bootstrap -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-bootstrap --config Release
```

3.  **Build Catalyst:**

```bash
git checkout dev-1.1.0
mkdir build
./build-bootstrap/catalyst build
```

4. Verify

```
./build/catalyst -v
```

## Verifying Installation

Run the following command to verify that Catalyst is installed correctly:

```bash
catalyst -v # should be 1.1.0 or later
```

## Next Steps

- [**Get Started**](getting_started.md).
