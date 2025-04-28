#include "builder.h"
#include "code.hpp"
#include <iostream>
#define REPOSITORY_SOURCE "https://github.com/"
#define REPOSITORY_TARGET_VENGEANCE "https://github.com/romanpunia/vengeance"
#define REPOSITORY_FILE_INDEX "addon.as"
#define REPOSITORY_FILE_ADDON "addon.json"

namespace asx
{
	static string format_directory_path(const std::string_view& value)
	{
		string new_value = "\"";
		new_value.append(value);
		if (new_value.back() == '\\')
			new_value.back() = '/';
		new_value += '\"';
		return new_value;
	}

	status_code builder::compile_into_addon(system_config& config, environment_config& env, virtual_machine* vm, const std::string_view& name, string& output)
	{
		string local_target = env.registry + string(name), remote_target = string(name.substr(1));
		if (is_directory_empty(local_target) && execute_git(config, "git clone " REPOSITORY_SOURCE + remote_target + " \"" + local_target + "\"") != status_code::OK)
		{
			VI_ERR("addon <%s> does not seem to be available at remote repository: <%s>", remote_target.c_str());
			return status_code::command_error;
		}

		uptr<schema> info = get_addon_info(env, name);
		if (!info)
		{
			VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file", name.data());
			return status_code::configuration_error;
		}

		string type = info->get_var("type").get_blob();
		if (type == "native")
		{
			if (!control::has(config, access_option::lib))
			{
				VI_ERR("addon <%s> cannot be created: permission denied", name.data());
				return status_code::configuration_error;
			}

			string build_directory = get_building_directory(env, local_target);
			string sh_local_target = format_directory_path(local_target);
			string sh_build_directory = format_directory_path(build_directory);
			os::directory::remove(build_directory.c_str());
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
			string configure_command = stringify::text("cmake -S %s -B %s", sh_local_target.c_str(), sh_build_directory.c_str());
#else
			string configure_command = stringify::text("cmake -S %s -B %s -DCMAKE_BUILD_TYPE=%s", sh_local_target.c_str(), sh_build_directory.c_str(), get_build_type(config));
#endif
			if (execute_cmake(config, configure_command) != status_code::OK)
			{
				VI_ERR("addon <%s> cannot be created: final target cannot be configured", name.data());
				return status_code::configuration_error;
			}
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
			string build_command = stringify::text("cmake --build %s --config %s", sh_build_directory.c_str(), get_build_type(config));
#else
			string build_command = stringify::text("cmake --build %s", sh_build_directory.c_str());
#endif
			if (execute_cmake(config, build_command) != status_code::OK)
			{
				VI_ERR("addon <%s> cannot be created: final target cannot be built", name.data());
				return status_code::build_error;
			}

			string addon_name = string(os::path::get_filename(name));
			string target_path = get_addon_target(env, name);
			string next_path = get_global_targets_directory(env, name) + VI_SPLITTER;
			os::directory::patch(next_path);

			vector<std::pair<string, file_entry>> files;
			string prev_path = get_local_targets_directory(env, name) + VI_SPLITTER;
			os::directory::scan(prev_path, files);
			for (auto& file : files)
			{
				string next_file_path = next_path + file.first;
				string prev_file_path = prev_path + file.first;
				if (!file.second.is_directory && stringify::starts_with(file.first, addon_name))
					os::file::move(prev_file_path.c_str(), next_file_path.c_str());
			}

			os::directory::remove(prev_path.c_str());
			return vm->import_addon(target_path) ? status_code::OK : status_code::dependency_error;
		}
		else if (type == "vm")
		{
			string index(info->get_var("index").get_blob());
			if (index.empty() || !stringify::ends_with(index, ".as") || stringify::find_of(index, "/\\").found)
			{
				VI_ERR("addon <%s> cannot be created: index file <%s> is not valid", name.data(), index.c_str());
				return status_code::configuration_error;
			}

			string path = local_target + VI_SPLITTER + index;
			if (!os::file::is_exists(path.c_str()))
			{
				VI_ERR("addon <%s> cannot be created: index file cannot be found", name.data());
				return status_code::configuration_error;
			}

			output = *os::file::read_as_string(path.c_str());
			return status_code::OK;
		}

		VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file: type <%s> is not recognized", name.data(), type.c_str());
		return status_code::configuration_error;
	}
	status_code builder::import_into_addon(environment_config& env, virtual_machine* vm, const std::string_view& name, string& output)
	{
		uptr<schema> info = get_addon_info(env, name);
		if (!info)
		{
			VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file", name.data());
			return status_code::configuration_error;
		}

		string type = info->get_var("type").get_blob();
		if (type == "native")
		{
			string path = get_addon_target(env, name);
			return vm->import_addon(path) ? status_code::OK : status_code::dependency_error;
		}
		else if (type == "vm")
		{
			string path = env.registry + string(name) + VI_SPLITTER + info->get_var("index").get_blob();
			if (!os::file::is_exists(path.c_str()))
			{
				VI_ERR("addon <%s> cannot be imported: index file cannot be found", name.data());
				return status_code::configuration_error;
			}

			output = *os::file::read_as_string(path.c_str());
			return status_code::OK;
		}

		VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file: type <%s> is not recognized", name.data(), type.c_str());
		return status_code::configuration_error;
	}
	status_code builder::initialize_into_addon(system_config& config, environment_config& env, virtual_machine* vm, const unordered_map<string, uint32_t>& settings)
	{
		if (!is_directory_empty(env.addon))
		{
			VI_ERR("cannot clone addon repository: target directory is not empty: %s", env.addon.c_str());
			return status_code::configuration_error;
		}

		unordered_map<string, string> keys = get_build_keys(config, env, vm, settings, true);
		if (env.mode == "vm")
		{
			vector<string> files =
			{
				"addon/addon.json",
				"addon/addon.as"
			};

			keys["BUILDER_INDEX"] = "\"" REPOSITORY_FILE_INDEX "\"";
			for (auto& file : files)
			{
				if (!append_template(keys, env.addon, file))
					return status_code::generation_error;
			}

			return status_code::OK;
		}
		else if (env.mode == "native")
		{
			unordered_map<string, string> files =
			{
				{ "addon/CMakeLists.txt", "" },
				{ "addon/addon.json", "" },
				{ "addon/addon.cpp", "" },
				{ "addon/interface.hpp", "" },
				{ "", "make" }
			};

			keys["BUILDER_INDEX"] = "null";
			for (auto& file : files)
			{
				string target_path = env.addon + file.second;
				if (file.first.empty())
				{
					if (!os::directory::create(target_path.c_str()))
					{
						VI_ERR("cannot generate the template in path: %s", target_path.c_str());
						return status_code::generation_error;
					}
				}
				else if (!append_template(keys, target_path, file.first))
					return status_code::generation_error;
			}

			return status_code::OK;
		}

		return status_code::configuration_error;
	}
	status_code builder::pull_addon_repository(system_config& config, environment_config& env)
	{
		if (env.registry.empty())
		{
			VI_ERR("provide entrypoint file to pull addons");
			return status_code::configuration_error;
		}

		vector<std::pair<string, file_entry>> entries;
		if (!os::directory::scan(env.registry.c_str(), entries) || entries.empty())
			return status_code::OK;

		auto pull = [&config](const std::string_view& path) { return execute_git(config, "cd \"" + string(path) + "\" && git pull") == status_code::OK; };
		for (auto& file : entries)
		{
			if (!file.second.is_directory || file.first.empty() || file.first.front() == '.')
				continue;

			if (file.first.front() == '@')
			{
				vector<std::pair<string, file_entry>> addons;
				string repositories_path = env.registry + file.first + VI_SPLITTER;
				if (!os::directory::scan(repositories_path.c_str(), addons) || addons.empty())
					continue;

				for (auto& addon : addons)
				{
					if (addon.second.is_directory && !pull(repositories_path + addon.first))
					{
						VI_ERR("cannot pull addon target repository: %s", file.first.c_str());
						return status_code::command_error;
					}
				}
			}
			else if (!pull(env.registry + file.first))
			{
				VI_ERR("cannot pull addon source repository: %s", file.first.c_str());
				return status_code::command_error;
			}
		}

		auto source_path = get_global_vitex_path();
		if (!is_directory_empty(source_path) && !pull(source_path))
		{
			VI_ERR("cannot pull source repository: %s", source_path.c_str());
			return status_code::command_error;
		}

		return status_code::OK;
	}
	status_code builder::compile_into_executable(system_config& config, environment_config& env, virtual_machine* vm, const unordered_map<string, uint32_t>& settings)
	{
		string vitex_directory = get_global_vitex_path();
		if (!append_vitex(config))
		{
			VI_ERR("cannot clone executable repository");
			return status_code::command_error;
		}

		unordered_map<string, string> keys = get_build_keys(config, env, vm, settings, false);
		unordered_map<string, string> files =
		{
			{ "executable/CMakeLists.txt", "" },
			{ "executable/vcpkg.json", "" },
			{ "executable/runtime.hpp", "" },
			{ "executable/program.cpp", "" },
			{ "", "make" }
		};

		for (auto& file : files)
		{
			string target_path = env.output + file.second;
			if (file.first.empty())
			{
				if (!os::directory::create(target_path.c_str()))
				{
					VI_ERR("cannot generate the template in path: %s", target_path.c_str());
					return status_code::generation_error;
				}
			}
			else if (!append_template(keys, target_path, file.first))
				return status_code::generation_error;
		}

		if (!append_byte_code(config, env, env.output + "program.b64"))
		{
			VI_ERR("cannot embed the byte code: make sure application has file read/write permissions");
			return status_code::byte_code_error;
		}

		if (!append_dependencies(env, vm, env.output + "bin/"))
		{
			VI_ERR("cannot embed the dependencies: make sure application has file read/write permissions");
			return status_code::configuration_error;
		}

		string sh_output_source = format_directory_path(env.output);
		string sh_output_build = format_directory_path(env.output + "make");
		string sh_vitex_directory = format_directory_path(vitex_directory);
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		string configure_command = stringify::text("cmake -S %s -B %s -DVI_DIRECTORY=%s -DVI_CXX=%i", sh_output_source.c_str(), sh_output_build.c_str(), sh_vitex_directory.c_str(), VI_CXX);
#else
		string configure_command = stringify::text("cmake -S %s -B %s -DVI_DIRECTORY=%s -DVI_CXX=%i -DCMAKE_BUILD_TYPE=%s", sh_output_source.c_str(), sh_output_build.c_str(), sh_vitex_directory.c_str(), VI_CXX, get_build_type(config));
#endif
		if (execute_cmake(config, configure_command) != status_code::OK)
		{
#ifdef VI_MICROSOFT
			VI_ERR("cannot configure an executable repository: make sure you have vcpkg installed");
#else
			VI_ERR("cannot configure an executable repository: make sure you have all dependencies installed");
#endif
			return status_code::configuration_error;
		}
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		string build_command = stringify::text("cmake --build %s --config %s", sh_output_build.c_str(), get_build_type(config));
#else
		string build_command = stringify::text("cmake --build %s", sh_output_build.c_str());
#endif
		if (execute_cmake(config, build_command) != status_code::OK)
		{
			VI_ERR("cannot build an executable repository");
			return status_code::build_error;
		}

		return status_code::OK;
	}
	unordered_map<string, uint32_t> builder::get_default_settings()
	{
		unordered_map<string, uint32_t> settings;
		settings["default_stack_size"] = (uint32_t)features::init_stack_size;
		settings["default_callstack_size"] = (uint32_t)features::init_call_stack_size;
		settings["callstack_size"] = (uint32_t)features::max_call_stack_size;
		settings["stack_size"] = (uint32_t)features::max_stack_size;
		settings["string_encoding"] = (uint32_t)features::string_encoding;
		settings["script_encoding_utf8"] = (uint32_t)features::script_scanner;
		settings["no_globals"] = (uint32_t)features::disallow_global_vars;
		settings["warnings"] = (uint32_t)features::compiler_warnings;
		settings["unicode"] = (uint32_t)features::allow_unicode_identifiers;
		settings["nested_calls"] = (uint32_t)features::max_nested_calls;
		settings["unsafe_references"] = (uint32_t)features::allow_unsafe_references;
		settings["optimized_bytecode"] = (uint32_t)features::optimize_bytecode;
		settings["copy_scripts"] = (uint32_t)features::copy_script_sections;
		settings["character_literals"] = (uint32_t)features::use_character_literals;
		settings["multiline_strings"] = (uint32_t)features::allow_multiline_strings;
		settings["implicit_handles"] = (uint32_t)features::allow_implicit_handle_types;
		settings["suspends"] = (uint32_t)features::build_without_line_cues;
		settings["init_globals"] = (uint32_t)features::init_global_vars_after_build;
		settings["require_enum_scope"] = (uint32_t)features::require_enum_scope;
		settings["jit_instructions"] = (uint32_t)features::include_jit_instructions;
		settings["accessor_mode"] = (uint32_t)features::property_accessor_mode;
		settings["array_template_message"] = (uint32_t)features::expand_def_array_to_tmpl;
		settings["automatic_gc"] = (uint32_t)features::auto_garbage_collect;
		settings["automatic_constructors"] = (uint32_t)features::always_impl_default_construct;
		settings["value_assignment_to_references"] = (uint32_t)features::disallow_value_assign_for_ref_type;
		settings["named_args_mode"] = (uint32_t)features::alter_syntax_named_args;
		settings["integer_division_mode"] = (uint32_t)features::disable_integer_division;
		settings["no_empty_list_elements"] = (uint32_t)features::disallow_empty_list_elements;
		settings["private_is_protected"] = (uint32_t)features::private_prop_as_protected;
		settings["heredoc_trim_mode"] = (uint32_t)features::heredoc_trim_mode;
		settings["generic_auto_handles_mode"] = (uint32_t)features::generic_call_mode;
		settings["ignore_shared_interface_duplicates"] = (uint32_t)features::ignore_duplicate_shared_intf;
		settings["ignore_debug_output"] = (uint32_t)features::no_debug_output;
		return settings;
	}
	string builder::get_system_version()
	{
		return to_string((size_t)vitex::major_version) + '.' + to_string((size_t)vitex::minor_version) + '.' + to_string((size_t)vitex::patch_version) + '.' + to_string((size_t)vitex::build_version);
	}
	status_code builder::execute_git(system_config& config, const std::string_view& command)
	{
		static int is_git_installed = -1;
		if (is_git_installed == -1)
		{
			is_git_installed = execute_command(config, "FIND", "git", 0x1) ? 1 : 0;
			if (!is_git_installed)
			{
				VI_ERR("cannot find <git> program, please make sure it is installed");
				return status_code::command_not_found;
			}
		}

		return execute_command(config, "RUN", command, 0x0) ? status_code::OK : status_code::command_error;
	}
	status_code builder::execute_cmake(system_config& config, const std::string_view& command)
	{
		static int is_cmake_installed = -1;
		if (is_cmake_installed == -1)
		{
			is_cmake_installed = execute_command(config, "FIND", "cmake", 0x0) ? 1 : 0;
			if (!is_cmake_installed)
			{
				VI_ERR("cannot find <cmake> program, please make sure it is installed");
				return status_code::command_not_found;
			}
		}

		return execute_command(config, "RUN", command, 0x0) ? status_code::OK : status_code::command_error;
	}
	bool builder::execute_command(system_config& config, const std::string_view& label, const std::string_view& command, int success_exit_code)
	{
		auto time = schedule::get_clock();
		auto* terminal = console::get();
		if (config.pretty_progress)
		{
			uint32_t window_size = 10, height = 0, y = 0;
			if (!terminal->read_screen(nullptr, &height, nullptr, &y))
				height = window_size;

			string stage = string(command);
			stringify::replace_in_between(stage, "\"", "\"", "<path>", false);
			uint32_t lines = std::min<uint32_t>(y >= --height ? 0 : y - height, window_size);
			bool logging = lines > 0 && label != "FIND", loading = true;
			uint64_t title = terminal->capture_element();
			uint64_t window = logging ? terminal->capture_window(lines) : 0;
			std::thread loader([terminal, title, &time, &loading, &label, &stage]()
			{
				while (loading)
				{
					auto diff = (schedule::get_clock() - time).count() / 1000000.0;
					terminal->spinning_element(title, "> " + string(label) + " " + stage + " - " + to_string(diff) + " seconds");
					std::this_thread::sleep_for(std::chrono::milliseconds(60));
				}
			});

			single_queue<string> messages;
			auto exit_code = os::process::execute(command, file_mode::read_only, [terminal, window, logging, &messages](const std::string_view& buffer)
			{
				size_t index = messages.size() + 1;
				string text = (index < 100 ? (index < 10 ? "[00" : "[0") : "[") + to_string(index) + "]  " + string(buffer);
				if (logging)
				{
					terminal->emplace_window(window, text);
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}
				messages.push(std::move(text));
				return true;
			});

			bool success = exit_code && *exit_code == success_exit_code;
			auto diff = (schedule::get_clock() - time).count() / 1000000.0;
			loading = false;
			loader.join();

			terminal->replace_element(title, "> " + string(label) + " " + stage + " - " + to_string(diff) + " seconds: " + (success ? string("OK") : (exit_code ? "EXIT " + to_string(*exit_code) : string("FAIL"))));
			terminal->free_element(title);
			if (logging)
				terminal->free_window(window, true);
			if (!success)
				terminal->write_line(">>> " + string(label) + " " + string(command));
			if (!exit_code)
				messages.push(exit_code.what() + "\n");

			while (!success && !messages.empty())
			{
				terminal->write(messages.front());
				messages.pop();
			}

			return success;
		}
		else
		{
			size_t messages = 0;
			terminal->write_line("> " + string(label) + " " + string(command) + ": PENDING");
			auto exit_code = os::process::execute(command, file_mode::read_only, [terminal, &messages](const std::string_view& buffer)
			{
				size_t index = ++messages;
				terminal->write((index < 100 ? (index < 10 ? "[00" : "[0") : "[") + to_string(index) + "]  " + string(buffer));
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				return true;
			});

			bool success = exit_code && *exit_code == success_exit_code;
			auto diff = (schedule::get_clock() - time).count() / 1000000.0;
			terminal->write_line("> " + string(label) + " " + string(command) + " - " + to_string(diff) + " seconds: " + (success ? string("OK") : (exit_code ? "EXIT " + to_string(*exit_code) : string("FAIL"))));
			if (!exit_code)
				terminal->write_line(exit_code.what());

			return success;
		}
	}
	bool builder::append_template(const unordered_map<string, string>& keys, const std::string_view& target_path, const std::string_view& template_path)
	{
		auto file = templates::fetch(keys, template_path);
		if (!file)
		{
			VI_ERR("cannot find the template: %s", template_path.data());
			return false;
		}

		if (!os::directory::patch(target_path))
		{
			VI_ERR("cannot generate the template in path: %s", target_path.data());
			return false;
		}

		string path = string(target_path);
		string filename = string(os::path::get_filename(template_path));
		if (path.back() != '/' && path.back() != '\\')
			path += VI_SPLITTER;

		if (!os::file::write(path + filename, (uint8_t*)file->data(), file->size()))
		{
			VI_ERR("cannot generate the template in path: %s - save failed", target_path.data());
			return false;
		}

		return true;
	}
	bool builder::append_byte_code(system_config& config, environment_config& env, const std::string_view& path)
	{
		byte_code_info info;
		info.debug = config.debug;
		if (!env.this_compiler->save_byte_code(&info))
		{
			VI_ERR("cannot fetch the byte code");
			return false;
		}

		os::directory::patch(os::path::get_directory(path));
		uptr<stream> target_file = os::file::open(path, file_mode::binary_write_only).or_else(nullptr);
		if (!target_file)
		{
			VI_ERR("cannot create the byte code file: %s", path.data());
			return false;
		}

		string data = codec::base64_encode(std::string_view((char*)info.data.data(), info.data.size()));
		if (target_file->write((uint8_t*)data.data(), data.size()).or_else(0) != data.size())
		{
			VI_ERR("cannot write the byte code file: %s", path.data());
			return false;
		}

		return true;
	}
	bool builder::append_dependencies(environment_config& env, virtual_machine* vm, const std::string_view& target_directory)
	{
		bool is_vm = false;
		for (auto& item : env.addons)
		{
			string from = get_addon_target_library(env, vm, item, &is_vm);
			if (is_vm)
				continue;

			string to = string(target_directory) + string(os::path::get_filename(from));
			if (!os::file::copy(from.c_str(), to.c_str()))
			{
				VI_ERR("cannot copy dependant addon: from: %s to: %s", from.c_str(), to.c_str());
				return false;
			}
		}

		return true;
	}
	bool builder::append_vitex(system_config& config)
	{
		string source_path = get_global_vitex_path();
		if (!is_directory_empty(source_path))
			return true;

		os::directory::patch(source_path);
		return execute_git(config, "git clone --recursive " REPOSITORY_TARGET_VENGEANCE " \"" + source_path + "\"") == status_code::OK;
	}
	bool builder::is_addon_target_exists(environment_config& env, virtual_machine* vm, const std::string_view& name, bool nested)
	{
		string local_target = string(nested ? name : get_addon_target(env, name));
		if (os::file::is_exists(local_target.c_str()))
			return true;

		for (auto& item : vm->get_compile_include_options().exts)
		{
			string local_target_ext = local_target + item;
			if (os::file::is_exists(local_target_ext.c_str()))
				return true;
		}

		if (nested)
			return false;

		uptr<schema> info = get_addon_info(env, name);
		if (info && info->get_var("type").get_blob() == "vm")
			return is_addon_target_exists(env, vm, env.registry + string(name) + VI_SPLITTER + info->get_var("index").get_blob(), true);

		return false;
	}
	bool builder::is_directory_empty(const std::string_view& target)
	{
		vector<std::pair<string, file_entry>> entries;
		return !os::directory::scan(target, entries) || entries.empty();
	}
	const char* builder::get_build_type(system_config& config)
	{
#ifndef NDEBUG
		return "Debug";
#else
		return (config.debug ? "RelWithDebInfo" : "Release");
#endif
	}
	string builder::get_global_vitex_path()
	{
#if VI_MICROSOFT
		string cache_directory = *os::directory::get_module();
		if (cache_directory.back() != '/' && cache_directory.back() != '\\')
			cache_directory += VI_SPLITTER;
		cache_directory += ".cache";
		cache_directory += VI_SPLITTER;
		cache_directory += "vengeance";
		return cache_directory;
#else
		return "/var/lib/asx/vengeance";
#endif

	}
	string builder::get_building_directory(environment_config& env, const std::string_view& local_target)
	{
		return env.registry + ".make";
	}
	string builder::get_local_targets_directory(environment_config& env, const std::string_view& name)
	{
		return stringify::text("%s%s%cbin", env.registry.c_str(), name.data(), VI_SPLITTER);
	}
	string builder::get_global_targets_directory(environment_config& env, const std::string_view& name)
	{
		string owner = string(name.substr(0, name.find('/')));
		string repository = string(os::path::get_filename(name));
		string path = env.registry + ".bin";
		path += VI_SPLITTER;
		path += owner;
		return path;
	}
	string builder::get_addon_target(environment_config& env, const std::string_view& name)
	{
		string owner = string(name.substr(0, name.find('/')));
		string repository = string(os::path::get_filename(name));
		string path = env.registry + ".bin";
		path += VI_SPLITTER;
		path += owner;
		path += VI_SPLITTER;
		path += repository;
		return path;
	}
	string builder::get_addon_target_library(environment_config& env, virtual_machine* vm, const std::string_view& name, bool* is_vm)
	{
		if (is_vm)
			*is_vm = false;

		string base_name = get_addon_target(env, name);
		auto result = os::path::resolve(base_name.c_str());
		string path1 = (result ? *result : base_name);
		if (os::file::is_exists(path1.c_str()))
			return path1;

		for (auto& ext : vm->get_compile_include_options().exts)
		{
			string path = path1 + ext;
			if (os::file::is_exists(path.c_str()))
				return path;
		}

		uptr<schema> info = get_addon_info(env, name);
		if (info->get_var("type").get_blob() != "vm")
			return path1;

		if (is_vm)
			*is_vm = true;

		string index = info->get_var("index").get_blob();
		path1 = env.registry + string(name) + VI_SPLITTER + index;
		result = os::path::resolve(path1.c_str());
		if (result)
			path1 = *result;
		return path1;
	}
	schema* builder::get_addon_info(environment_config& env, const std::string_view& name)
	{
		string local_target = env.registry + string(name) + VI_SPLITTER + REPOSITORY_FILE_ADDON;
		auto data = os::file::read_as_string(local_target);
		if (!data)
			return nullptr;

		auto result = schema::from_json(*data);
		return result ? *result : nullptr;
	}
	unordered_map<string, string> builder::get_build_keys(system_config& config, environment_config& env, virtual_machine* vm, const unordered_map<string, uint32_t>& settings, bool is_addon)
	{
		string config_permissions_array;
		for (auto& item : config.permissions)
			config_permissions_array += stringify::text("{ access_option::%s, %s }, ", os::control::to_string(item.first), item.second ? "true" : "false");

		string config_settings_array;
		for (auto& item : settings)
		{
			size_t value = vm->get_property((features)item.second);
			config_settings_array += stringify::text("{ (uint32_t)%i, (size_t)%" PRIu64 " }, ", item.second, (uint64_t)value);
		}

		string config_system_addons_array;
		for (auto& item : vm->get_system_addons())
		{
			if (item.second.exposed)
				config_system_addons_array += stringify::text("\"%s\", ", item.first.c_str());
		}

		string config_libraries_array, config_functions_array;
		for (auto& item : vm->get_clibraries())
		{
			config_libraries_array += stringify::text("{ \"%s\", %s }, ", item.first.c_str(), item.second.is_addon ? "true" : "false");
			if (item.second.is_addon)
				continue;

			for (auto& function : item.second.functions)
				config_functions_array += stringify::text("{ \"%s\", { \"%s\", \"%s\" } }, ", item.first.c_str(), function.first.c_str());
		}

		auto* lib = vitex::heavy_runtime::get();
		bool is_using_shaders = lib->has_ft_shaders() && !is_addon && vm->has_system_addon("graphics");
		bool is_using_open_gl = lib->has_so_opengl() && !is_addon && vm->has_system_addon("graphics");
		bool is_using_open_al = lib->has_so_openal() && !is_addon && vm->has_system_addon("audio");
		bool is_using_open_ssl = lib->has_so_openssl() && !is_addon && (vm->has_system_addon("crypto") || vm->has_system_addon("network"));
		bool is_using_sdl2 = lib->has_so_sdl2() && !is_addon && vm->has_system_addon("activity");
		bool is_using_glew = lib->has_so_glew() && !is_addon && vm->has_system_addon("graphics");
		bool is_using_spirv = lib->has_so_spirv() && !is_addon && vm->has_system_addon("graphics");
		bool is_using_zlib = lib->has_so_zlib() && !is_addon && (vm->has_system_addon("fs") || vm->has_system_addon("codec"));
		bool is_using_assimp = lib->has_so_assimp() && !is_addon && vm->has_system_addon("engine");
		bool is_using_mongo_db = lib->has_so_mongoc() && !is_addon && vm->has_system_addon("mongodb");
		bool is_using_postgre_sql = lib->has_so_postgresql() && !is_addon && vm->has_system_addon("postgresql");
		bool is_using_sq_lite = lib->has_so_sqlite() && !is_addon && vm->has_system_addon("sqlite");
		bool is_using_rml_ui = lib->has_md_rmlui() && !is_addon && vm->has_system_addon("ui");
		bool is_using_free_type = lib->has_so_freetype() && !is_addon && vm->has_system_addon("ui");
		bool is_using_bullet3 = lib->has_md_bullet3() && !is_addon && vm->has_system_addon("physics");
		bool is_using_tiny_file_dialogs = lib->has_md_tinyfiledialogs() && !is_addon && vm->has_system_addon("activity");
		bool is_using_stb = lib->has_md_stb() && !is_addon && vm->has_system_addon("engine");
		bool is_using_pugi_xml = lib->has_md_pugixml() && !is_addon && vm->has_system_addon("schema");
		bool is_using_rapid_json = lib->has_md_rapidjson() && !is_addon && vm->has_system_addon("schema");
		vector<std::pair<string, bool>> features =
		{
			{ "ALLOCATOR", lib->has_ft_allocator() },
			{ "PESSIMISTIC", lib->has_ft_pessimistic() },
			{ "BINDINGS", lib->has_ft_bindings() && !is_addon },
			{ "FCONTEXT", lib->has_ft_fcontext() && !is_addon },
			{ "BACKWARDCPP", lib->has_md_backwardcpp() && !is_addon },
			{ "WEPOLL", lib->has_md_wepoll() && !is_addon },
			{ "VECTORCLASS", lib->has_md_vectorclass() && !is_addon },
			{ "ANGELSCRIPT", lib->has_md_angelscript() && !is_addon },
			{ "SHADERS", is_using_shaders },
			{ "OPENGL", is_using_open_gl },
			{ "OPENAL", is_using_open_al },
			{ "OPENSSL", is_using_open_ssl },
			{ "SDL2", is_using_sdl2 },
			{ "GLEW", is_using_glew },
			{ "SPIRV", is_using_spirv },
			{ "ZLIB", is_using_zlib },
			{ "ASSIMP", is_using_assimp },
			{ "MONGOC", is_using_mongo_db },
			{ "POSTGRESQL", is_using_postgre_sql },
			{ "SQLITE", is_using_sq_lite },
			{ "RMLUI", is_using_rml_ui },
			{ "FREETYPE", is_using_free_type },
			{ "BULLET3", is_using_bullet3 },
			{ "TINYFILEDIALOGS", is_using_tiny_file_dialogs },
			{ "STB", is_using_stb },
			{ "PUGIXML", is_using_pugi_xml },
			{ "RAPIDJSON", is_using_rapid_json }
		};

		string feature_list;
		for (auto& item : features)
			feature_list += stringify::text("set(VI_%s %s CACHE BOOL \"-\")\n", item.first.c_str(), item.second ? "ON" : "OFF");

		if (!feature_list.empty())
			feature_list.erase(feature_list.end() - 1);

		schema* config_install_array = var::set::array();
		if (is_using_spirv)
		{
			config_install_array->push(var::string("spirv-cross"));
			config_install_array->push(var::string("glslang"));
		}
		if (is_using_zlib)
			config_install_array->push(var::string("zlib"));
		if (is_using_assimp)
			config_install_array->push(var::string("assimp"));
		if (is_using_free_type)
			config_install_array->push(var::string("freetype"));
		if (is_using_sdl2)
			config_install_array->push(var::string("sdl2"));
		if (is_using_open_al)
			config_install_array->push(var::string("openal-soft"));
		if (is_using_glew)
			config_install_array->push(var::string("glew"));
		if (is_using_open_ssl)
			config_install_array->push(var::string("openssl"));
		if (is_using_mongo_db)
			config_install_array->push(var::string("mongo-c-driver"));
		if (is_using_postgre_sql)
			config_install_array->push(var::string("libpq"));
		if (is_using_sq_lite)
			config_install_array->push(var::string("sqlite3"));

		string vitex_path = get_global_vitex_path();
		stringify::replace(vitex_path, '\\', '/');

		unordered_map<string, string> keys;
		keys["BUILDER_ENV_AUTO_SCHEDULE"] = to_string(env.auto_schedule);
		keys["BUILDER_ENV_AUTO_CONSOLE"] = env.auto_console ? "true" : "false";
		keys["BUILDER_ENV_AUTO_STOP"] = env.auto_stop ? "true" : "false";
		keys["BUILDER_CONFIG_INSTALL"] = schema::to_json(config_install_array);
		keys["BUILDER_CONFIG_PERMISSIONS"] = config_permissions_array;
		keys["BUILDER_CONFIG_SETTINGS"] = config_settings_array;
		keys["BUILDER_CONFIG_LIBRARIES"] = config_libraries_array;
		keys["BUILDER_CONFIG_FUNCTIONS"] = config_functions_array;
		keys["BUILDER_CONFIG_ADDONS"] = config_system_addons_array;
		keys["BUILDER_CONFIG_TAGS"] = config.tags ? "true" : "false";
		keys["BUILDER_CONFIG_TS_IMPORTS"] = config.ts_imports ? "true" : "false";
		keys["BUILDER_CONFIG_ESSENTIALS_ONLY"] = config.essentials_only ? "true" : "false";
		keys["BUILDER_VENGEANCE_URL"] = config_system_addons_array;
		keys["BUILDER_VENGEANCE_PATH"] = vitex_path;
		keys["BUILDER_APPLICATION"] = env.auto_console ? "OFF" : "ON";
		keys["BUILDER_FEATURES"] = feature_list;
		keys["BUILDER_VERSION"] = get_system_version();
		keys["BUILDER_MODE"] = env.mode;
		keys["BUILDER_OUTPUT"] = env.name.empty() ? "build_target" : env.name;
		keys["BUILDER_STANDARD"] = to_string(VI_CXX);
		return keys;
	}

	bool control::has(system_config& config, access_option option)
	{
		auto it = config.permissions.find(option);
		return it != config.permissions.end() ? it->second : os::control::has(option);
	}

	option<string> templates::fetch(const unordered_map<string, string>& keys, const std::string_view& path)
	{
		if (!files)
		{
#ifdef HAS_CODE_BUNDLE
			files = memory::init<unordered_map<string, string>>();
			code_bundle::foreach(nullptr, [](void*, const char* path, const char* file, unsigned int file_size)
			{
				files->insert(std::make_pair(string(path), string(file, file_size)));
			});
#else
			return optional::none;
#endif
		}

		auto it = files->find(key_lookup_cast(path));
		if (it == files->end())
			return optional::none;

		string result = it->second;
		for (auto& value : keys)
			stringify::replace(result, "{{" + value.first + "}}", value.second);
		return result;
	}
	void templates::cleanup()
	{
		memory::deinit(files);
	}
	unordered_map<string, string>* templates::files = nullptr;
}