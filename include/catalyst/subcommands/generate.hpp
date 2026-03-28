#pragma once
#include <expected>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <CLI/App.hpp>
#include <yaml-cpp/yaml.h>

namespace catalyst::generate {
struct Parse {
    std::vector<std::string> profiles;
    std::vector<std::string> enabled_features;
    std::string backend;
};

struct FindRes {
    std::string lib_path;
    std::string inc_path;
    std::string libs;
};

std::expected<YAML::Node, std::string> profileComposition(const std::vector<std::string> &profiles);
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
std::expected<void, std::string> action(const Parse &);

std::expected<std::string, std::string> libPath(const YAML::Node &profile,
                                                 const std::vector<std::string> &profiles);
std::expected<FindRes, std::string> findDep(const std::string &build_dir, const YAML::Node &dep);
std::expected<FindRes, std::string> findLocal(const YAML::Node &dep);
std::expected<FindRes, std::string> findSystem(const YAML::Node &dep);
std::expected<FindRes, std::string> findVcpkg(const YAML::Node &dep);
std::expected<FindRes, std::string> findGit(const std::string &build_dir, const YAML::Node &dep);

std::expected<std::unordered_set<std::filesystem::path>, std::string>
buildSourceSet(const std::vector<std::string> &source_dirs, const std::vector<std::string> &profiles);

namespace buildwriters {

struct WriterVariable {
    std::string_view name;
    std::string_view value;
};

struct WriterRule {
    std::string_view name;
    std::string_view command;
    std::string_view description;
    std::string_view depfile;
    std::string_view deps;
};

struct WriterBuild {
    std::vector<std::string> outputs;
    std::string_view rule;
    std::vector<std::string> inputs;
    std::vector<std::string> implicit_deps;
};

class BaseWriter {
protected:
    std::ostream &stream;
    explicit BaseWriter(std::ostream &stream) : stream(stream) {
    }

public:
    BaseWriter(const BaseWriter &) = default;
    BaseWriter(BaseWriter &&) = delete;
    BaseWriter &operator=(const BaseWriter &) = delete;
    BaseWriter &operator=(BaseWriter &&) = delete;
    virtual ~BaseWriter() = default;

    virtual std::expected<void, std::string> addVariable(std::string_view name, std::string_view value) = 0;
    virtual std::expected<void, std::string> addRule(std::string_view name,
                                                     std::string_view command,
                                                     std::string_view description,
                                                     std::string_view depfile = "",
                                                     std::string_view deps = "") = 0;
    virtual std::expected<void, std::string> addBuild(const std::vector<std::string> &outputs,
                                                      std::string_view rule,
                                                      const std::vector<std::string> &inputs,
                                                      const std::vector<std::string> &implicit_deps = {}
                                                      // e.g., headers for validation
                                                      ) = 0;

    virtual void addComment(std::string_view comment) = 0;
    virtual void addDefault(std::string_view target) = 0;
};

enum class TargetType : std::uint8_t {
    Ninja,
    Make,
    VisualStudio,
    XCode,
    CBE,
};

template <TargetType Target_T> class DerivedWriter : public BaseWriter {
private:
    static consteval bool isImplemented(TargetType t) {
        return t == TargetType::Ninja || t == TargetType::CBE || t == TargetType::Make;
    }

#if __cplusplus >= 202602L
    static consteval std::string warningMsg(TargetType __t) {
        switch (__t) {
            case TargetType::Ninja:
                return "Unimplemented specialization for DerivedWriter<Ninja>. "
                       "Add explicit template specialization.";
            case TargetType::Make:
                return "Unimplemented specialization for DerivedWriter<Make>. "
                       "Add explicit template specialization.";
            case TargetType::VisualStudio:
                return "Unimplemented specialization for DerivedWriter<VisualStudio>. "
                       "Add explicit template specialization.";
            case TargetType::XCode:
                return "Unimplemented specialization for DerivedWriter<XCode>. "
                       "Add explicit template specialization.";
            case TargetType::CBE:
                return "Unimplemented specialization for DerivedWriter<CBE>. "
                       "Add explicit template specialization.";
        }
        return "Unknown TargetType";
    }
    static_assert(isImplemented(Target_T), warningMsg(Target_T));
#else
    static_assert(isImplemented(Target_T),
                  "Unimplemented specialization for DerivedWriter. Add explicit template specialization.");
#endif

public:
    explicit DerivedWriter(std::ostream &stream) : BaseWriter(stream) {
    }

    ~DerivedWriter() override = default;

    // Delete copy/move to prevent slicing
    DerivedWriter(const DerivedWriter &) = delete;
    DerivedWriter &operator=(const DerivedWriter &) = delete;
    DerivedWriter(DerivedWriter &&) = delete;
    DerivedWriter &operator=(DerivedWriter &&) = delete;

    std::expected<void, std::string> addVariable([[maybe_unused]] std::string_view name,
                                                 [[maybe_unused]] std::string_view value) override {
        throw std::logic_error("Unimplemented base template method");
    }

    std::expected<void, std::string> addRule([[maybe_unused]] std::string_view name,
                                             [[maybe_unused]] std::string_view command,
                                             [[maybe_unused]] std::string_view description,
                                             [[maybe_unused]] std::string_view depfile = "",
                                             [[maybe_unused]] std::string_view deps = "") override {
        throw std::logic_error("Unimplemented base template method");
    }

    std::expected<void, std::string>
    addBuild([[maybe_unused]] const std::vector<std::string> &outputs,
             [[maybe_unused]] std::string_view rule,
             [[maybe_unused]] const std::vector<std::string> &inputs,
             [[maybe_unused]] const std::vector<std::string> &implicit_deps = {}) override {
        throw std::logic_error("Unimplemented base template method");
    }

    void addComment([[maybe_unused]] std::string_view comment) override {
        throw std::logic_error("Unimplemented base template method");
    }

    void addDefault([[maybe_unused]] std::string_view target) override {
        throw std::logic_error("Unimplemented base template method");
    }
};
} // namespace buildwriters
} // namespace catalyst::generate
