#include <filesystem>
#include <map>
#include <memory>
#include <CLI/App.hpp>

#include "catalyst/subcommands/pack.hpp"

namespace catalyst::pack {

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app) {
    CLI::App *pack = app.add_subcommand("pack", "Assemble the local package for distribution.");
    auto ret = std::make_unique<Parse>();
    pack->add_option("-p,--profiles", ret->profiles, "the profiles to compose for packing")
        ->default_val(std::vector<std::string>{"common"});
    pack->add_option("-s,--source", ret->source_path, "the source path to pack")
        ->default_val(std::filesystem::current_path());
    pack->add_option("-t,--target", ret->target_path, "the output directory for the package")
        ->default_val(std::filesystem::path{"build/pack"});
    
    std::map<std::string, Generator> gen_map{
        {"TGZ", Generator::TGZ},
        {"ZIP", Generator::ZIP},
        {"DEB", Generator::DEB},
        {"RPM", Generator::RPM}
    };
    pack->add_option("-G,--generators", ret->generators, "CPack generators to use")
        ->transform(CLI::CheckedTransformer(gen_map, CLI::ignore_case));
        
    return {pack, std::move(ret)};
}
} // namespace catalyst::pack
