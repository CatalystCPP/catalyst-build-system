#pragma once
#include <expected>
#include <memory>
#include <string>
#include <optional>

#include <CLI/App.hpp>
#include "catalyst/utils/yaml/configuration.hpp"

namespace catalyst::doc {
struct Parse {
    std::optional<std::string> engine;
    bool open = false;
    bool clean = false;
};

std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &parse_args);

enum class DocEngine : std::uint8_t {
    Doxygen,
    ClangDoc,
    Jocasta
};

class BaseRunner {
protected:
    const Parse &opts;
    const catalyst::utils::yaml::Configuration &config;

public:
    BaseRunner(const Parse &opts, const catalyst::utils::yaml::Configuration &config)
        : opts(opts), config(config) {}
    virtual ~BaseRunner() = default;

    virtual std::expected<void, std::string> run() = 0;
};

template <DocEngine DocEngine_T>
class DerivedRunner : public BaseRunner {
private:
    static consteval bool isImplemented(DocEngine t) {
        return t == DocEngine::Doxygen || t == DocEngine::ClangDoc || t == DocEngine::Jocasta;
    }

#if __cplusplus >= 202602L
    static consteval std::string warningMsg(DocEngine __t) {
        switch (__t) {
            case DocEngine::Doxygen:
                return "Unimplemented specialization for DerivedRunner<Doxygen>. "
                       "Add explicit template specialization.";
            case DocEngine::ClangDoc:
                return "Unimplemented specialization for DerivedRunner<ClangDoc>. "
                       "Add explicit template specialization.";
            case DocEngine::Jocasta:
                return "Unimplemented specialization for DerivedRunner<Jocasta>. "
                       "Add explicit template specialization.";
        }
        return "Unknown DocEngine";
    }
    static_assert(isImplemented(DocEngine_T), warningMsg(DocEngine_T));
#else
    static_assert(isImplemented(DocEngine_T),
                  "Unimplemented specialization for DerivedRunner. Add explicit template specialization.");
#endif

public:
    using BaseRunner::BaseRunner;
    ~DerivedRunner() override = default;

    // Delete copy/move to prevent slicing
    DerivedRunner(const DerivedRunner &) = delete;
    DerivedRunner &operator=(const DerivedRunner &) = delete;
    DerivedRunner(DerivedRunner &&) = delete;
    DerivedRunner &operator=(DerivedRunner &&) = delete;

    std::expected<void, std::string> run() override {
        throw std::logic_error("Unimplemented base template method");
    }
};

} // namespace catalyst::doc
