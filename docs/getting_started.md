# Getting Started

If you haven't already, [Install Catalyst](installation.md)

This guide will walk you through creating, building, and running your first C++ project with Catalyst.

## 1. Initialize a Project

Create a new directory for your project and initialize it:

```bash
mkdir my-app
cd my-app
catalyst init
```

This generates a basic project structure:

```
.
├── catalyst.yaml           <-- the configuration for the common profile.
├── tc_catalyst.yaml        <-- a default catalyst toolchain configuration.
├── build/                  <-- the build output directory.
├── src/                    <-- directory for source files.
│   ├── .catalystignore     <-- a catalystignore file.
│   └── my-app.cpp          <-- a default entry point.
└── include/                <-- a default include directory.
```

## 2. Build the Project

Run the build command:

```bash
catalyst build
```

This will:

1.  Generate build files.
2.  Compile your source code.
3.  Link the executable.

Artifacts are stored in the `build/common/` directory.

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
