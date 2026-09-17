# macOS CI Bootstrap and Self-Hosting

`.github/workflows/macos.yml` builds and tests Catalyst on Apple Silicon (`arm64-osx`)
runners.

## Supported Configuration and Prerequisites

- **Architecture**: Apple Silicon (`arm64`, Blacksmith macOS runner).
- **Operating System**: macOS 14.0+ (Sonoma, Sequoia).
- **Deployment Target**: `-mmacosx-version-min=14.0`.
- **Compiler**: LLVM Clang/Clang++ >= 21 (`brew install llvm@21`).
- **Standard Library**: LLVM libc++ with C++23 features enabled (`-stdlib=libc++ -D_LIBCPP_DISABLE_AVAILABILITY`).
- **Build Tools**: CMake 3.25+, Ninja, ccache, pkg-config, vcpkg (`arm64-osx` triplet).

### Required C++23 Features
Catalyst requires full C++23 support including:
- `std::expected` / `std::unexpected`
- `std::format`
- `std::println` / `std::print`
- `std::views::join_with` and C++23 ranges
- `<span>`

These features are verified at configure time by `scripts/ci/CMakeLists.txt`.

## Build Stages

1. **Prerequisites Installation**:
   Install LLVM 21, CMake, Ninja, ccache, and vcpkg dependencies:
   ```bash
   brew install llvm@21 cmake ninja ccache pkg-config wget
   vcpkg install reproc ryml efsw catch2 yaml-cpp --triplet arm64-osx
   bash scripts/fetch_cli_11.sh
   ```

2. **Native Bootstrap**:
   CMake compiles Catalyst and the pinned COB binary (`build/macos/catalyst`, `build/macos/cob`).
   ```bash
   cmake -S scripts/ci -B build/macos -G Ninja \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
     -DVCPKG_TARGET_TRIPLET="arm64-osx" \
     -DCOB_SOURCE_DIR="$PWD/build/cob-source"
   cmake --build build/macos --parallel
   ctest --test-dir build/macos --output-on-failure
   ```

3. **Self-Hosting Build**:
   The bootstrap binary fetches dependencies and builds Catalyst using the native macOS profiles
   and standalone toolchains (`tc_catalyst_macos.yaml`, `tc_catalyst_macos_debug.yaml`):
   ```bash
   export COB_PATH="$PWD/build/macos/cob"
   # Fetch dependencies for release and tests
   build/macos/catalyst fetch --profiles common ccache macos
   build/macos/catalyst fetch --profiles common ccache macos test macos-test

   # Build with COB backend
   build/macos/catalyst build --backend cob --profiles common ccache macos

   # Build with Ninja backend
   build/macos/catalyst build --backend ninja --regen --profiles common ccache macos
   ```

4. **Self-Hosted Verification**:
   The newly built self-hosted binary runs the unit test suite and smoke-tests sample projects:
   ```bash
   # Build and run unit test suite
   build/common-ccache-macos/catalyst build --backend cob --profiles common ccache macos test macos-test
   build/test/common-ccache-macos-test-macos-test/catalyst_tests

   # Verify debug and release build coexistence
   build/common-ccache-macos/catalyst build --backend cob --profiles common ccache macos-debug
   ```

## Shared-Library Integration Test

The macOS workflow runs this test with the self-hosted Catalyst binary:

```bash
python3 scripts/ci/test_macos_shared_library.py \
  --catalyst build/common-ccache-macos/catalyst
```

For both COB and Ninja, it scaffolds a shared library and a consumer without
editing either generated toolchain, checks the Mach-O `.dylib`, and builds and
runs the consumer through a local dependency. It then installs the library,
header, and executable, deletes the source/build trees, and verifies installed
and relocated execution from an unrelated working directory. Removing the
installed dylib must cause a loader failure (the negative control).

**Loader contract:** `catalyst run` supplies the build-tree library search path.
Installed and relocated execution explicitly sets `DYLD_LIBRARY_PATH` to the
prefix's `lib` directory. This verifies installation and relocation with an
explicit loader search path; it does **not** claim automatic relocatability.
Package-relative `@rpath`/install-name handling remains a separate integration
milestone. Inherited loader environment variables are cleared before testing.

Changes to `tc_catalyst*.yaml` also trigger macOS CI.

## Native Toolchains and Profiles

- `tc_catalyst_macos.yaml`: Native Apple Silicon release toolchain with Mach-O linker flags (`-Wl,-dead_strip`, `-dynamiclib`), `.dylib` extensions, and no ELF/mold/static flags.
- `tc_catalyst_macos_debug.yaml`: Native Apple Silicon debug toolchain (`-O0 -g -DDEBUG`).
- `macos`: Manifest profile selecting `tc_catalyst_macos.yaml` and `arm64-osx` dependencies.
- `macos-debug`: Manifest profile selecting `tc_catalyst_macos_debug.yaml`.
- `macos-test`: Test dependency profile providing `catch2` and `yaml-cpp` for `arm64-osx`.
- `scripts/resolve_cob.py`: Resolves and stages native COB executable into `build/tools/cob` without altering mtimes for incremental builds.
