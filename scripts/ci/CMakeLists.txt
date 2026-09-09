cmake_minimum_required(VERSION 3.25)
project(catalyst_macos_ci LANGUAGES CXX)

# CI bootstrap only: the normal project build remains Catalyst-managed.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(CATALYST_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT IS_DIRECTORY "${COB_SOURCE_DIR}/src")
    message(FATAL_ERROR "COB_SOURCE_DIR must point to the pinned COB checkout")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
find_package(reproc++ CONFIG REQUIRED)
find_package(ryml CONFIG REQUIRED)
find_package(efsw CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

# COB's Linux process implementation must not be compiled on Darwin.
file(GLOB_RECURSE COB_SOURCES CONFIGURE_DEPENDS "${COB_SOURCE_DIR}/src/*.cpp")
list(FILTER COB_SOURCES EXCLUDE REGEX "/process_exec_linux\\.cpp$")
add_executable(cob ${COB_SOURCES})
target_include_directories(cob PRIVATE "${COB_SOURCE_DIR}/include")
target_link_libraries(cob PRIVATE reproc++ Threads::Threads)
file(STRINGS "${COB_SOURCE_DIR}/CATALYST.yaml" COB_VERSION_LINE
     REGEX "^    version: " LIMIT_COUNT 1)
string(REGEX REPLACE "^    version: *" "" COB_VERSION "${COB_VERSION_LINE}")
target_compile_definitions(cob PRIVATE
    CATALYST_BUILD_SYS=1 CATALYST_PROJ_NAME="cob" CATALYST_PROJ_VER="${COB_VERSION}"
    FF_cob__binary=1
    FF_cob__progress_queue_size=8192
    FF_cob__buffer_initial_capacity=2097152
    FF_cob__flush_threshold=1048576
    FF_cob__rsp_input_threshold=50
    FF_cob__rsp_path_estimate=100
    FF_cob__heavy_task_threshold=100
    FF_cob__map_initial_capacity=16
    FF_cob__clean_ring_capacity=64)

# Keep generated bootstrap sources in the build tree. Reuse the normal generators.
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}/include/catalyst")
add_custom_command(
    OUTPUT "${GENERATED_DIR}/include/catalyst/cob_embedded.hpp"
           "${GENERATED_DIR}/src/utils/cob_embedded.cpp"
    COMMAND Python3::Interpreter "${CATALYST_SOURCE_DIR}/scripts/embed_cob.py"
            --cob-path "$<TARGET_FILE:cob>"
    WORKING_DIRECTORY "${GENERATED_DIR}"
    DEPENDS cob "${CATALYST_SOURCE_DIR}/scripts/embed_cob.py")
foreach(generator IN ITEMS dispatch hooks)
    add_custom_command(
        OUTPUT "${GENERATED_DIR}/include/catalyst/${generator}.hpp"
        COMMAND Python3::Interpreter "${CATALYST_SOURCE_DIR}/scripts/generate_${generator}.py"
                -o "${GENERATED_DIR}/include/catalyst/${generator}.hpp"
        DEPENDS "${CATALYST_SOURCE_DIR}/scripts/generate_${generator}.py")
endforeach()

file(GLOB_RECURSE CATALYST_SOURCES CONFIGURE_DEPENDS "${CATALYST_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM CATALYST_SOURCES "${CATALYST_SOURCE_DIR}/src/catalyst.cpp"
                               "${CATALYST_SOURCE_DIR}/src/utils/cob_embedded.cpp")
add_library(catalyst_core STATIC ${CATALYST_SOURCES}
    "${GENERATED_DIR}/src/utils/cob_embedded.cpp"
    "${GENERATED_DIR}/include/catalyst/dispatch.hpp"
    "${GENERATED_DIR}/include/catalyst/hooks.hpp")
target_include_directories(catalyst_core PUBLIC
    "${GENERATED_DIR}/include"
    "${CATALYST_SOURCE_DIR}/include"
    "${CATALYST_SOURCE_DIR}/include/extern")
file(STRINGS "${CATALYST_SOURCE_DIR}/CATALYST.yaml" VERSION_LINE
     REGEX "^    version: " LIMIT_COUNT 1)
string(REGEX REPLACE "^    version: *" "" CATALYST_VERSION "${VERSION_LINE}")
target_compile_definitions(catalyst_core PUBLIC
    CATALYST_BUILD_SYS=1 CATALYST_PROJ_VER="${CATALYST_VERSION}"
    CATALYST_PROJ_NAME="catalyst")
target_link_libraries(catalyst_core PUBLIC reproc++ ryml::ryml efsw::efsw Threads::Threads)

add_executable(catalyst "${CATALYST_SOURCE_DIR}/src/catalyst.cpp")
target_link_libraries(catalyst PRIVATE catalyst_core)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CATALYST_SOURCE_DIR}/tests/*.cpp")
add_executable(catalyst_tests ${TEST_SOURCES})
target_link_libraries(catalyst_tests PRIVATE catalyst_core Catch2::Catch2 yaml-cpp::yaml-cpp)
enable_testing()
add_test(NAME catalyst_tests COMMAND catalyst_tests)
set_tests_properties(catalyst_tests PROPERTIES TIMEOUT 300)
