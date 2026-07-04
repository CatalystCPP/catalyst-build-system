#pragma once
#include <expected>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <CLI11.hpp>
#include <ryml/ryml.hpp>

#include "catalyst/utils/result.hpp"
#include "catalyst/utils/toolchain.hpp"
#include "catalyst/utils/yaml/configuration.hpp"

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
    std::vector<std::string> lib_dirs;
};

Result<utils::yaml::Configuration> profileComposition(const std::vector<std::string> &profiles);
std::pair<CLI::App *, std::unique_ptr<Parse>> parse(CLI::App &app);
Result<void> action(const Parse &);

Result<std::string> libPath(const utils::yaml::Configuration &profile_comp,
                            const std::vector<std::string> &profiles,
                            const catalyst::toolchain::ToolchainDef &tc);
Result<FindRes>
findDep(const std::string &build_dir, ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc);
Result<FindRes> findLocal(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc);
Result<FindRes> findSystem(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc);
Result<FindRes> findVcpkg(ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc);

/// Parsed view of vcpkg's installed-package database
/// ($VCPKG_ROOT/installed/vcpkg/status), which is in Debian-control format.
struct VcpkgStatusDb {
    /// Maps an installed package to its direct dependency names. The key is
    /// "<name>\x1f<triplet>"; build it with vcpkgStatusKey(). Dependencies from
    /// a package's base stanza and all its installed feature stanzas are unioned.
    std::unordered_map<std::string, std::vector<std::string>> depends;
};

/// Key into VcpkgStatusDb::depends for a (package, triplet) pair.
std::string vcpkgStatusKey(std::string_view name, std::string_view triplet);

/// Parses the contents of a vcpkg `status` file. Only stanzas marked
/// "install ok installed" are recorded; dependency tokens are normalized
/// (feature/platform qualifiers like `pkg[feat]` or `pkg (windows)` stripped).
VcpkgStatusDb parseVcpkgStatus(std::string_view status_content);

/// Returns the transitive vcpkg dependencies of @p root_name (excluding
/// @p root_name itself) for @p triplet, in a valid static-link order
/// (a package always precedes the packages it depends on). @p is_real_port
/// gates both emission and recursion, so build-time helper ports that produce
/// no link/include artifacts are pruned. Diamonds and cycles are deduped.
std::vector<std::string>
vcpkgTransitiveClosure(const VcpkgStatusDb &db,
                       const std::string &root_name,
                       const std::string &triplet,
                       const std::function<bool(const std::string &name, const std::string &triplet)> &is_real_port);
Result<FindRes>
findGit(const std::string &build_dir, ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc);
Result<FindRes>
findConan(const std::string &build_dir, ryml::ConstNodeRef dep, const catalyst::toolchain::ToolchainDef &tc);

struct FeatureFlag {
    std::string name;
    std::string type; // "bool", "enum", "int", "string"
    std::string default_val;
    std::vector<std::string> enum_values;
    std::vector<std::string> files;

    std::string resolved_val;
    bool is_enabled = false;
};

Result<std::vector<FeatureFlag>> resolveFeatureFlags(const utils::yaml::Configuration &config,
                                                     const std::vector<std::string> &enabled_features);

Result<std::unordered_set<std::filesystem::path>> buildSourceSet(const std::vector<std::string> &source_dirs,
                                                                 const std::vector<std::string> &profiles);

namespace modules {

/// What one C++ TU provides/requires in the module graph, learned from a
/// P1689 dependency scan.
struct ModuleInfo {
    std::string provides;              ///< logical module name; empty if the TU is not an interface unit
    std::vector<std::string> required; ///< logical names of imported modules
};

/// Scan result over the whole source set.
struct ScanResult {
    std::unordered_map<std::filesystem::path, ModuleInfo> tu_modules; ///< only TUs that touch modules
    std::unordered_map<std::string, std::filesystem::path> providers; ///< module name -> interface unit source
};

/// Parses one clang-scan-deps -format=p1689 JSON document (P1689r5).
Result<ModuleInfo> parseP1689(std::string_view json);

/// True for the canonical module-interface extensions (.cppm/.ixx/.mpp/.cxxm).
bool isInterfaceExtension(const std::filesystem::path &src);

/// True when Clang needs an explicit `-x c++-module` to treat @p src as an
/// interface unit (it only recognizes .cppm/.cxxm natively).
bool needsExplicitModuleType(const std::filesystem::path &src);

/// BMI output path Catalyst chooses for a module name (obj/<name>.pcm).
std::string bmiPath(std::string_view module_name);

/// Runs `clang-scan-deps -format=p1689` over every C++ TU in @p source_set,
/// reusing @p cxxflags (the exact flags the compile steps will get). Skipped
/// entirely (empty result) when the set contains no module-interface unit, so
/// projects without modules never need clang-scan-deps installed. A required
/// module with no provider in the set is an error.
Result<ScanResult> scanModules(const std::unordered_set<std::filesystem::path> &source_set,
                               const catalyst::toolchain::ToolchainDef &tc,
                               std::string_view cxxflags);

} // namespace modules

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

    virtual Result<void> addVariable(std::string_view name, std::string_view value) = 0;
    virtual Result<void> addRule(std::string_view name,
                                 std::string_view command,
                                 std::string_view description,
                                 std::string_view depfile = "",
                                 std::string_view deps = "") = 0;
    virtual Result<void> addBuild(const std::vector<std::string> &outputs,
                                  std::string_view rule,
                                  const std::vector<std::string> &inputs,
                                  const std::vector<std::string> &implicit_deps = {} // e.g., headers for validation
                                  ) = 0;

    virtual void addComment(std::string_view comment) = 0;
    virtual void addDefault(std::string_view target) = 0;
};

enum class TargetType : std::uint8_t {
    Ninja,
    Make,
    VisualStudio,
    XCode,
    COB,
};

template <TargetType Target_T> class DerivedWriter : public BaseWriter {
private:
    static consteval bool isImplemented(TargetType t) {
        return t == TargetType::Ninja || t == TargetType::COB || t == TargetType::Make;
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
            case TargetType::COB:
                return "Unimplemented specialization for DerivedWriter<COB>. "
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

    Result<void> addVariable([[maybe_unused]] std::string_view name, [[maybe_unused]] std::string_view value) override {
        throw std::logic_error("Unimplemented base template method");
    }

    Result<void> addRule([[maybe_unused]] std::string_view name,
                         [[maybe_unused]] std::string_view command,
                         [[maybe_unused]] std::string_view description,
                         [[maybe_unused]] std::string_view depfile = "",
                         [[maybe_unused]] std::string_view deps = "") override {
        throw std::logic_error("Unimplemented base template method");
    }

    Result<void> addBuild([[maybe_unused]] const std::vector<std::string> &outputs,
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
