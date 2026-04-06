#include "catalyst/utils/toolchain.hpp"

namespace catalyst::toolchain {

std::string expand_template(std::string_view tmpl, const std::unordered_map<std::string_view, std::string> &vars) {
    std::string result;
    result.reserve(tmpl.size());
    size_t i = 0;
    while (i < tmpl.size()) {
        if (tmpl[i] == '{') {
            size_t end = tmpl.find('}', i);
            if (end != std::string_view::npos) {
                std::string_view key = tmpl.substr(i + 1, end - i - 1);
                if (auto it = vars.find(key); it != vars.end()) {
                    result.append(it->second);
                } else {
                    // Placeholder not found, keep it as is or expand to empty.
                    // Let's expand to empty, as missing flags should not leave {placeholder}
                }
                i = end + 1;
                continue;
            }
        }
        result.push_back(tmpl[i]);
        ++i;
    }
    return result;
}

} // namespace catalyst::toolchain
