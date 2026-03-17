#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "catalyst/subcommands/init.hpp"
#include "catalyst/utils/log/log.hpp"
#include "catalyst/utils/os/os_defs.hpp"

namespace catalyst::init {

namespace fs = std::filesystem;

namespace {
std::expected<std::ofstream, std::string> createFile(const fs::path &file_path, bool force) {
    if (fs::exists(file_path) && !force) {
        const std::string filename{file_path.filename().string()};
        catalyst::logger.log(LogLevel::WARN, "{} already exists.\nUse --force-ide to overwrite.", filename);
        return std::unexpected(std::format("{} already exists", filename));
    }
    std::ofstream out{file_path};
    if (!out) {
        return std::unexpected(std::format("Failed to create {}", file_path.string()));
    }
    return out;
}
} // namespace

std::expected<void, std::string> invokeIDEConfigEmitters(const Parse &parse_args) {
    for (const Parse::IdeType &ide : parse_args.ides) {
        std::function<std::expected<void, std::string>(const Parse &)> emit_fn;
        switch (ide) {
            case Parse::IdeType::vsc:
                emit_fn = emitIDEConfig<Parse::IdeType::vsc>;
                break;
            case Parse::IdeType::clion:
                emit_fn = emitIDEConfig<Parse::IdeType::clion>;
                break;
            default:
                continue;
        }
        if (auto res = emit_fn(parse_args); !res) {
            return std::unexpected(std::format("Failed to emit IDE configuration. Error: {}", res.error()));
        }
    }
    return {};
}

template <> std::expected<void, std::string> emitIDEConfig<Parse::IdeType::vsc>(const Parse &parse_args) {
    catalyst::logger.log(LogLevel::INFO, "Generating VS Code IDE configuration");

    catalyst::utils::os::OSInfo os_info{};
    std::string os_str{"linux"};
    std::string arch_str{"x64"};
    std::string mi_mode{"gdb"};
    std::string exe_ext;

    if (os_info.os == catalyst::utils::os::OperatingSystem::Windows) {
        os_str = "windows";
        exe_ext = ".exe";
    } else if (os_info.os == catalyst::utils::os::OperatingSystem::MacOS) {
        os_str = "macos";
        mi_mode = "lldb";
    }

    if (os_info.arch == catalyst::utils::os::Architecture::x86) {
        arch_str = "x86";
    } else if (os_info.arch == catalyst::utils::os::Architecture::ARM64) {
        arch_str = "arm64";
    } else if (os_info.arch == catalyst::utils::os::Architecture::ARM32) {
        arch_str = "arm";
    }

    const std::string intellisense_mode = std::format("{}-clang-{}", os_str, arch_str);

    const fs::path vscode_dir{parse_args.path / ".vscode"};
    if (!fs::exists(vscode_dir)) {
        fs::create_directories(vscode_dir);
    }

    // c_cpp_properties.json
    {
        const fs::path cpp_props_path{vscode_dir / "c_cpp_properties.json"};
        auto cpp_props{createFile(cpp_props_path, parse_args.force_emit_ide)};
        if (!cpp_props) {
            return std::unexpected(cpp_props.error());
        }
        *cpp_props << std::format(R"json({{
    "configurations": [
        {{
            "name": "Catalyst",
            "compileCommands": "${{workspaceFolder}}/build/compile_commands.json",
            "intelliSenseMode": "{}"
        }}
    ],
    "version": 4
}}
)json",
                                  intellisense_mode);
    }

    // tasks.json
    {
        const fs::path tasks_path{vscode_dir / "tasks.json"};
        auto tasks{createFile(tasks_path, parse_args.force_emit_ide)};
        if (!tasks) {
            return std::unexpected(tasks.error());
        }
        *tasks << R"json({
    "version": "2.0.0",
    "tasks": [
        {
            "label": "catalyst build",
            "type": "shell",
            "command": "catalyst build",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "catalyst run",
            "type": "shell",
            "command": "catalyst run",
            "problemMatcher": []
        },
        {
            "label": "catalyst test",
            "type": "shell",
            "command": "catalyst test",
            "group": "test",
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "catalyst clean",
            "type": "shell",
            "command": "catalyst clean",
            "problemMatcher": []
        }
    ]
}
)json";
    }

    // launch.json
    {
        const fs::path launch_path{vscode_dir / "launch.json"};
        auto launch{createFile(launch_path, parse_args.force_emit_ide)};
        if (!launch) {
            return std::unexpected(launch.error());
        }
        *launch << std::format(R"json({{
    "version": "0.2.0",
    "configurations": [
        {{
            "name": "Debug (Catalyst)",
            "type": "cppdbg",
            "request": "launch",
            "program": "${{workspaceFolder}}/build/{}{}",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${{workspaceFolder}}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "{}",
            "preLaunchTask": "catalyst build"
        }}
    ]
}}
)json",
                               parse_args.name,
                               exe_ext,
                               mi_mode);
    }

    // settings.json
    {
        const fs::path settings_path{vscode_dir / "settings.json"};
        auto settings{createFile(settings_path, parse_args.force_emit_ide)};
        if (!settings) {
            return std::unexpected(settings.error());
        }
        *settings << R"json({
    "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json",
    "C_Cpp.formatting": "clangFormat",
    "editor.formatOnSave": true
}
)json";
    }

    return {};
}

template <> std::expected<void, std::string> emitIDEConfig<Parse::IdeType::clion>(const Parse &parse_args) {
    catalyst::logger.log(LogLevel::INFO, "Generating CLion IDE configuration");

    catalyst::utils::os::OSInfo os_info{};
    std::string exe_ext{""};
    if (os_info.os == catalyst::utils::os::OperatingSystem::Windows) {
        exe_ext = ".exe";
    }

    const fs::path idea_dir{parse_args.path / ".idea"};
    const fs::path run_configs_dir{idea_dir / "runConfigurations"};
    const fs::path tools_dir{idea_dir / "tools"};

    if (!fs::exists(run_configs_dir)) {
        fs::create_directories(run_configs_dir);
    }
    if (!fs::exists(tools_dir)) {
        fs::create_directories(tools_dir);
    }

    // External Tools
    {
        const fs::path tools_xml_path{tools_dir / "External Tools.xml"};
        auto tools_xml{createFile(tools_xml_path, parse_args.force_emit_ide)};
        if (!tools_xml) {
            return std::unexpected(tools_xml.error());
        }
        *tools_xml << R"xml(<toolSet name="External Tools">
  <tool name="catalyst build" description="Build project with Catalyst" showInMainMenu="false" showInEditor="false" showInProject="false" showInSearchPopup="false" disabled="false" useConsole="true" showConsoleOnStdOut="false" showConsoleOnStdErr="false" synchronizeAfterRun="true">
    <exec>
      <option name="COMMAND" value="catalyst" />
      <option name="PARAMETERS" value="build" />
      <option name="WORKING_DIRECTORY" value="$ProjectFileDir$" />
    </exec>
  </tool>
  <tool name="catalyst clean" description="Clean project with Catalyst" showInMainMenu="false" showInEditor="false" showInProject="false" showInSearchPopup="false" disabled="false" useConsole="true" showConsoleOnStdOut="false" showConsoleOnStdErr="false" synchronizeAfterRun="true">
    <exec>
      <option name="COMMAND" value="catalyst" />
      <option name="PARAMETERS" value="clean" />
      <option name="WORKING_DIRECTORY" value="$ProjectFileDir$" />
    </exec>
  </tool>
</toolSet>
)xml";
    }

    // Run Configuration
    {
        const fs::path run_xml_path{run_configs_dir / "Catalyst.xml"};
        auto run_xml{createFile(run_xml_path, parse_args.force_emit_ide)};
        if (!run_xml) {
            return std::unexpected(run_xml.error());
        }
        *run_xml << std::format(R"xml(<component name="ProjectRunConfigurationManager">
  <configuration default="false" name="Catalyst Run/Debug" type="CLionNativeAppRunConfigurationType" REDIRECT_INPUT="false" ELEVATE="false" USE_EXTERNAL_CONSOLE="false" PASS_PARENT_ENVS_2="true" PROJECT_NAME="{0}" TARGET_NAME="{0}" CONFIG_NAME="{0}" version="1" RUN_PATH="$PROJECT_DIR$/build/{0}{1}">
    <method v="2">
      <option name="ToolBeforeRunTask" enabled="true" actionId="Tool_External Tools_catalyst build" />
    </method>
  </configuration>
</component>
)xml",
                                parse_args.name,
                                exe_ext);
    }

    return {};
}

} // namespace catalyst::init
