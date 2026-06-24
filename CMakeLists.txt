cmake_minimum_required(VERSION 3.15)
project(catalyst CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Fetch CLI11 if missing
set(CLI11_DEST "${CMAKE_SOURCE_DIR}/include/extern/CLI11.hpp")
if(NOT EXISTS "${CLI11_DEST}")
    message(STATUS "Downloading CLI11.hpp to ${CLI11_DEST}...")
    file(DOWNLOAD "https://github.com/CLIUtils/CLI11/releases/download/v2.6.2/CLI11.hpp" "${CLI11_DEST}")
endif()

# 2. Provide dummy cob_embedded for bootstrapping if missing
set(COB_HEADER "${CMAKE_SOURCE_DIR}/include/catalyst/cob_embedded.hpp")
set(COB_SOURCE "${CMAKE_SOURCE_DIR}/src/utils/cob_embedded.cpp")
if(NOT EXISTS "${COB_HEADER}")
    file(WRITE "${COB_HEADER}" "#pragma once\n#include <cstddef>\nnamespace catalyst::embedded {\nextern const unsigned char cob_binary[];\nextern const std::size_t cob_binary_len;\n}\n")
endif()
if(NOT EXISTS "${COB_SOURCE}")
    file(WRITE "${COB_SOURCE}" "#include \"catalyst/cob_embedded.hpp\"\nnamespace catalyst::embedded {\nalignas(16) const unsigned char cob_binary[] = {0};\nconst std::size_t cob_binary_len = 0;\n}\n")
endif()

# 3. Dependencies via vcpkg
# Make sure to configure CMake with:
# -DCMAKE_TOOLCHAIN_FILE=[vcpkg_root]/scripts/buildsystems/vcpkg.cmake
find_package(reproc++ CONFIG REQUIRED)
find_package(ryml CONFIG REQUIRED)

# 4. Gather sources
file(GLOB_RECURSE CATALYST_SOURCES "src/*.cpp")

# 5. Define target
add_executable(catalyst ${CATALYST_SOURCES})

target_include_directories(catalyst PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/extern"
)

target_link_libraries(catalyst PRIVATE
    reproc++
    ryml::ryml
)

# 6. Build Info Macros (Usually provided by Catalyst itself)
target_compile_definitions(catalyst PRIVATE
    CATALYST_BUILD_SYS="CMake"
    CATALYST_PROJ_NAME="catalyst"
    CATALYST_PROJ_VER="1.8.0-dev"
)

# 7. Compiler flags
if(MSVC)
    target_compile_options(catalyst PRIVATE /W4 /Zc:preprocessor)
else()
    target_compile_options(catalyst PRIVATE -Wall -Wextra -Wpedantic)
endif()
