#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <CLI/App.hpp>
#include <CLI/Validators.hpp>

#include "catalyst/subcommands/init.hpp"

namespace catalyst::init {
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *init = app.add_subcommand("init", "Initialize a new catalyst profile.");
    auto ret = std::make_unique<Parse>();
    init->add_option("-n,--name", ret->name, "the name of the project")
        ->default_str(std::filesystem::current_path().filename().string());
    init->add_option("--path", ret->path, "the default path for the project")
        ->default_val(std::filesystem::current_path());

    const std::map<std::string, Parse::Type> type_map{{"binary", Parse::Type::BINARY},
                                                      {"staticlib", Parse::Type::STATICLIB},
                                                      {"sharedlib", Parse::Type::SHAREDLIB},
                                                      {"interface", Parse::Type::INTERFACE}};
    init->add_option("-t,--type", ret->type, "the project type binary, staticlib, sharedlib, or interface")
        ->transform(CLI::CheckedTransformer(type_map, CLI::ignore_case).description(""))
        ->default_str("binary");

    init->add_option("-v,--version", ret->version, "the project's version")->default_str("0.0.1");

    init->add_option("-d,--description", ret->description, "a description for the project")
        ->default_str("Your Description Goes Here.");
    init->add_option("--provides", ret->provides, "Artifact provided by this project.")->default_str("");

#ifdef _WIN32
    init->add_option("--cc", ret->tooling.cc, "the c compiler to use")->default_str("msvc");
    init->add_option("--cxx", ret->tooling.cxx, "the cxx compiler to use")->default_str("msvc");
#else

    init->add_option("--cc", ret->tooling.cc, "the c compiler to use")->default_str("cc");
    init->add_option("--cxx", ret->tooling.cxx, "the cxx compiler to use")->default_str("c++");
#endif // _WIN32
    init->add_option("--ccflags", ret->tooling.ccflags, "c compiler flags")->default_str("");
    init->add_option("--cxxflags", ret->tooling.cxxflags, "cxx compiler flags")->default_str("");
    init->add_option("--ldflags", ret->tooling.ldflags, "linker flags")->default_str("");

    init->add_option("--include-dirs", ret->dirs.include, "include directories")
        ->default_val(std::vector<std::string>{"include"});
    init->add_option("--source-dirs", ret->dirs.source, "source directories")
        ->default_val(std::vector<std::string>{"src"});
    init->add_option("--build-dir", ret->dirs.build, "build directory")->default_str("build");

    const std::map<std::string, Parse::IdeType> ide_map{
        {"vscode", Parse::IdeType::vsc},
        {"clion", Parse::IdeType::clion},
    };
    init->add_option("--ides", ret->ides, "IDEs to generate project files for")
        ->transform(CLI::CheckedTransformer(ide_map, CLI::ignore_case));
    init->add_option("-p,--profile", ret->profile, "the profile to initialize")->default_val("common");
    init->add_flag("-f,--force-ide", ret->force_emit_ide, "force emitting IDE config even if one already exists")
        ->default_val(false);

    return {init, std::move(ret)};
}
} // namespace catalyst::init
