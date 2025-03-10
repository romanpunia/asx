#ifndef BUILDER_H
#define BUILDER_H
#include "runtime.hpp"
#include <vengeance/vengeance.h>

namespace asx
{
	enum class status_code
	{
		OK = 0,
		command_error = 1,
		command_not_found = 2,
		generation_error = 3,
		byte_code_error = 4,
		dependency_error = 5,
		configuration_error = 6,
		build_error = 7
	};

	class builder
	{
	public:
		static status_code compile_into_addon(system_config& config, environment_config& env, virtual_machine* vm, const std::string_view& name, string& output);
		static status_code import_into_addon(environment_config& env, virtual_machine* vm, const std::string_view& name, string& output);
		static status_code initialize_into_addon(system_config& config, environment_config& env, virtual_machine* vm, const unordered_map<string, uint32_t>& settings);
		static status_code pull_addon_repository(system_config& config, environment_config& env);
		static status_code compile_into_executable(system_config& config, environment_config& env, virtual_machine* vm, const unordered_map<string, uint32_t>& settings);
		static unordered_map<string, uint32_t> get_default_settings();
		static string get_system_version();
		static string get_addon_target_library(environment_config& env, virtual_machine* vm, const std::string_view& name, bool* is_vm);
		static bool is_addon_target_exists(environment_config& env, virtual_machine* vm, const std::string_view& name, bool nested = false);

	private:
		static status_code execute_git(system_config& config, const std::string_view& command);
		static status_code execute_cmake(system_config& config, const std::string_view& command);
		static bool execute_command(system_config& config, const std::string_view& label, const std::string_view& command, int success_exit_code);
		static bool append_template(const unordered_map<string, string>& keys, const std::string_view& target_path, const std::string_view& template_path);
		static bool append_byte_code(system_config& config, environment_config& env, const std::string_view& path);
		static bool append_dependencies(environment_config& env, virtual_machine* vm, const std::string_view& target_directory);
		static bool append_vitex(system_config& config);
		static bool is_directory_empty(const std::string_view& target);
		static const char* get_build_type(system_config& config);
		static string get_global_vitex_path();
		static string get_building_directory(environment_config& env, const std::string_view& local_target);
		static string get_global_targets_directory(environment_config& env, const std::string_view& name);
		static string get_local_targets_directory(environment_config& env, const std::string_view& name);
		static string get_addon_target(environment_config& env, const std::string_view& name);
		static schema* get_addon_info(environment_config& env, const std::string_view& name);
		static unordered_map<string, string> get_build_keys(system_config& config, environment_config& env, virtual_machine* vm, const unordered_map<string, uint32_t>& settings, bool is_addon);
	};

	class control
	{
	public:
		static bool has(system_config& config, access_option option);
	};

	class templates
	{
	private:
		static unordered_map<string, string>* files;

	public:
		static option<string> fetch(const unordered_map<string, string>& keys, const std::string_view& path);
		static void cleanup();
	};
}
#endif