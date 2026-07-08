#include <sstream>
#include <string>

#include "catalyst/subcommands/generate.hpp"
#include "catalyst/utils/toolchain.hpp"

namespace catalyst::generate {

void appendPkgConfigFlags(const std::string &flags,
                          const catalyst::toolchain::ToolchainDef &tc,
                          PkgConfigFlagBucket fallback,
                          FindRes &result) {
    std::stringstream stream(flags);
    std::string token;
    while (stream >> token) {
        if (token.starts_with("-I")) {
            result.inc_path += " " + catalyst::toolchain::expandTemplate(tc.flags.include_dir, {{"path", token.substr(2)}});
        } else if (token.starts_with("-L")) {
            std::string path = token.substr(2);
            result.lib_path += " " + catalyst::toolchain::expandTemplate(tc.flags.lib_dir, {{"path", path}});
            result.lib_dirs.push_back(path);
        } else if (token.starts_with("-l")) {
            result.libs += " " + catalyst::toolchain::expandTemplate(tc.flags.lib, {{"name", token.substr(2)}});
        } else {
            switch (fallback) {
                case PkgConfigFlagBucket::Compile:
                    result.inc_path += " " + token;
                    break;
                case PkgConfigFlagBucket::LinkDirs:
                    result.lib_path += " " + token;
                    break;
                case PkgConfigFlagBucket::Link:
                    result.libs += " " + token;
                    break;
            }
        }
    }
}

} // namespace catalyst::generate
