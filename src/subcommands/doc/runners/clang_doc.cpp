#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "catalyst/subcommands/doc.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/process_exec.hpp"

namespace catalyst::doc {

template <>
std::expected<void, std::string> DerivedRunner<DocEngine::ClangDoc>::run() {
    catalyst::logger.info("Running clang-doc documentation engine");

    std::string out_dir = config.getString("manifest.tooling.doc.out_dir").value_or("docs/");

    if (opts.clean) {
        if (std::filesystem::exists(out_dir)) {
            catalyst::logger.info("Cleaning output directory: {}", out_dir);
            std::filesystem::remove_all(out_dir);
        }
    }

    std::string compile_commands = "build/compile_commands.json";
    if (!std::filesystem::exists(compile_commands)) {
        return std::unexpected("compile_commands.json not found in build/ directory. Please run `catalyst generate` first.");
    }

    auto res = catalyst::processExec({"clang-doc", "--executor=all-TUs", compile_commands, "--output=" + out_dir, "--format=md"}, std::filesystem::current_path().string());

    if (!res || res.value().get() != 0) {
        return std::unexpected("clang-doc failed. Make sure clang-doc is installed and the project compiles.");
    }

    if (opts.open) {
        std::string index_path = (std::filesystem::path(out_dir) / "index.md").string();
        if (std::filesystem::exists(index_path)) {
            catalyst::logger.info("Opening {}", index_path);
    #ifdef __APPLE__
            catalyst::processExec({"open", index_path}, std::filesystem::current_path().string());
    #elif __linux__
            catalyst::processExec({"xdg-open", index_path}, std::filesystem::current_path().string());
    #elif _WIN32
            catalyst::processExec({"start", index_path}, std::filesystem::current_path().string());
    #endif
        } else {
            catalyst::logger.warn("Could not find index.html to open.");
        }
    }

    return {};
}

template class DerivedRunner<DocEngine::ClangDoc>;

} // namespace catalyst::doc
