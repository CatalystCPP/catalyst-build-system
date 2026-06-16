#include <expected>
#include <filesystem>
#include <fstream>
#include <string>

#include <catalyst/utils/yaml/profile_write_back.hpp>

#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/result.hpp"
#include "catalyst/utils/yaml/ryml_utils.hpp"

namespace catalyst::utils::yaml {
Result<void> profileWriteBack(const ProfileFile &profile_file) {
    catalyst::logger.debug("Writing profile file: {}", profile_file.path.string());
    std::ofstream profile_file_out{profile_file.path};
    std::string text = emitYaml(profile_file.tree);
    profile_file_out << text;
    if (!text.ends_with('\n'))
        profile_file_out << '\n';
    catalyst::logger.debug("Profile file written successfully.");
    return {};
}
} // namespace catalyst::utils::yaml
