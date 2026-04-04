#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "catalyst/process_exec.hpp"
#include "catalyst/subcommands/doc.hpp"
#include "catalyst/utils/log/log.hpp"

namespace catalyst::doc {

template <> std::expected<void, std::string> DerivedRunner<DocEngine::Doxygen>::run() {
    catalyst::logger.info("Running Doxygen documentation engine");

    std::string config_file = config.getString("manifest.tooling.doc.config").value_or("Doxyfile");
    std::string out_dir = config.getString("manifest.tooling.doc.out_dir").value_or("docs/");
    catalyst::logger.debug("Using Doxygen config file: {}/{}", out_dir, config_file);

    if (opts.clean) {
        if (std::filesystem::exists(out_dir)) {
            catalyst::logger.info("Cleaning output directory: {}", out_dir);
            std::filesystem::remove_all(out_dir);
        }
    }

    bool created_tmp_config = false;
    if (!std::filesystem::exists(config_file)) {
        catalyst::logger.warn("Config file {} not found. Generating temporary configuration.", config_file);
        // A minimal Doxyfile content could be generated here
        // For simplicity, we create a basic one.
        std::string tmp_config = "INPUT = src/ include/\nOUTPUT_DIRECTORY = " + out_dir + "\n";
        FILE *f = fopen("Doxyfile.tmp", "w");
        if (f) {
            fwrite(tmp_config.c_str(), 1, tmp_config.size(), f);
            fclose(f);
        }
        config_file = "Doxyfile.tmp";
        created_tmp_config = true;
    }

    auto res = catalyst::processExec({"doxygen", config_file}, std::filesystem::current_path().string());

    int exit_code = 1;
    if (res) {
        exit_code = res.value().get();
    }

    if (created_tmp_config) {
        std::filesystem::remove("Doxyfile.tmp");
    }

    if (!res || exit_code != 0) {
        return std::unexpected("Doxygen failed");
    }

    if (opts.open) {
        std::string index_path = (std::filesystem::path(out_dir) / "html" / "index.html").string();
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

template class DerivedRunner<DocEngine::Doxygen>;

} // namespace catalyst::doc
