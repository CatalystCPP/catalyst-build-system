# macOS CI bootstrap

`.github/workflows/macos.yml` builds and tests on Apple Silicon and Intel without
uploading artifacts or creating releases. It follows the Linux workflow's
checkout, ccache, dependency-fetch, and build stages, but uses this CMake bootstrap
because the Linux job's preinstalled Catalyst and COB packages are `.deb` files.

The bootstrap compiles the current Catalyst sources and unit suite, generates
headers with the existing scripts, and embeds a native COB executable. COB is
pinned to 0.7.1, which includes the macOS compatibility fixes. The bootstrap
excludes its Linux process implementation and supplies its project name and
version from its manifest. No downstream source patch is needed.

The workflow runs unit tests and `init`, `build`, and `run` smoke tests with both
Ninja and COB. It does not yet validate Catalyst self-hosting through the root
Linux-specific `CATALYST.yaml`/toolchain, packaging, or the Python end-to-end suite.
LLVM 21's libc++ is used at compile and runtime, so these are CI binaries, not
self-contained distribution artifacts.
