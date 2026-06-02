# Getting Started

This guide will walk you through creating, building, and running your first C++ project with Catalyst.

## 1. Initialize a Project

Create a new directory for your project and initialize it:

```bash
mkdir my-app
cd my-app
catalyst init
```

This generates a basic project structure:

- `catalyst.yaml`: The project configuration file.
- `src/`: Directory for source files.
    - ``src/my-app.cpp``: A default entry point.
- `include/`: Directory for header files.

## 2. Build the Project

Run the build command:

```bash
catalyst build
```

This will:
1.  Generate build files (using COB by default, or optionally Ninja/GNU Make).
2.  Compile your source code.
3.  Link the executable.

Artifacts are stored in the `build/` directory.

## 3. Run the Application

Execute the new binary:

```bash
catalyst run
```

You should see:
```
Hello, Catalyst!
```

## Next Steps

- **Add Dependencies**: Learn how to add libraries with [`catalyst add`](cli/add.md).
- **Configure Profiles**: Set up debug and release builds in [Profiles](concepts/profiles.md).
- **Explore Config**: Check out the [Configuration Guide](concepts/configuration.md).
