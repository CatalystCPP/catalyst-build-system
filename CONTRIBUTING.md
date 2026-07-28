# Contributing to Catalyst

Thank you for considering contributing to Catalyst.

Catalyst is currently in its early stages of development.

- The interface and configuration schema may change frequently.
- We are actively looking for feedback and bug reports.
- Large architectural changes are still possible.

## Contributing

### Bug Reports

- **Search first:** Check the [GitHub Issues](https://github.com/CatalystCPP/catalyst-build-system/issues) to see if the bug has already been reported.
- **Be specific:** Include your OS, compiler version, and a minimal reproducible example (a small `catalyst.yaml` and source file).
- **Logs:** Provide the exact error message and any relevant output (the entire ``.catalyst.log`` output not just command line output).

### Suggesting Enhancements

- **Open an issue:** Before writing code for a major feature, please open an issue to discuss it. We want to ensure it aligns with the project's goals.
- **Explain the "Why":** Describe the problem you're trying to solve and how the proposed feature addresses it.

### Pull Requests

1. **Fork the repo** and create your branch from `dev`.
2. **Coding Style:** Ensure your code follows the project's style (see below).
3. **Documentation:** If you're adding a feature or changing a subcommand, update the relevant files in the `docs/` directory.
4. Keep it atomic: Try to keep PRs focused on a single change, a general rule of thumb is ~100 loc.

## Development Setup

### Prerequisites

- **C++ Compiler:** Clang++ (preferred) or GCC with C++23 support.
- **Dependencies:** The project depends on `yaml-cpp`, `CLI11`, `reproc`, and `vcpkg`.

### Building Catalyst

Catalyst is designed to build itself. If you already have a `catalyst` binary:

```bash
catalyst build --profiles debug
```

If you are building for the first time, please refer to the [Installation guide](installation.md) for bootstrapping instructions.

## Style Guide

- Use modern C++ features (C++23 and beyond).
- Use the lowest level of abstraction that is still cross-platform.
- Follow RAII principles.

We use `clang-format` based on the LLVM style. Please run `clang-format` on your changes before
committing (run ``catalyst fmt``).

Refer to the `.clang-format` file in the root directory for exact rules.

## Project Structure

- `src/`: Implementation files.
- `include/`: Header files.
- `src/subcommands/`: Logic for each `catalyst` subcommand (e.g., `build`, `init`, `add`).
- `docs/`: Documentation (MkDocs format).
- `tests/`: Unit and integration tests.

## Release and Support Policy

See the [release and support policy](README.md#release-and-support-policy) prior to contributing.

## License

By contributing to Catalyst, you agree that your contributions will be licensed under the [Apache 2.0 License](LICENSE.md).
