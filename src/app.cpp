#include "app.h"
#include <signal.h>

namespace asx
{
	environment::environment(int args_count, char** args) : loop(nullptr), vm(nullptr), context(nullptr), unit(nullptr)
	{
		add_default_commands();
		add_default_settings();
		listen_for_signals();
		env.parse(args_count, args, flags);
		error_handling::set_flag(log_option::report_sys_errors, false);
		error_handling::set_flag(log_option::active, true);
		error_handling::set_flag(log_option::pretty, false);
		config.essentials_only = !env.commandline.has("engine", "e");
		config.install = env.commandline.has("install", "i") || env.commandline.has("target");
#ifndef NDEBUG
		os::directory::set_working(os::directory::get_module()->c_str());
		config.save_source_code = true;
#endif
	}
	environment::~environment()
	{
		templates::cleanup();
		if (console::has_instance())
			console::get()->detach();
		memory::release(context);
		memory::release(unit);
		memory::release(vm);
		memory::release(loop);
		printf("\n");
	}
	int environment::dispatch()
	{
		auto* terminal = console::get();
		terminal->attach();

		vm = new virtual_machine();
		bindings::heavy_registry().bind_addons(vm);
		for (auto& next : env.commandline.args)
		{
			if (next.first == "__path__")
				continue;

			auto* command = find_argument(next.first);
			if (!command)
			{
				VI_ERR("command <%s> is not a valid operation", next.first.c_str());
				return (int)exit_status::invalid_command;
			}

			int exit_code = command->callback(next.second);
			if (exit_code != (int)exit_status::next)
				return exit_code;
		}

		if (!env.commandline.params.empty())
		{
			string directory = *os::directory::get_working();
			auto file = os::path::resolve(env.commandline.params.front(), directory, true);
			if (!file || !os::file::get_state(*file, &env.file) || env.file.is_directory)
			{
				file = os::path::resolve(env.commandline.params.front() + (config.load_byte_code ? ".as.gz" : ".as"), directory, true);
				if (file && os::file::get_state(*file, &env.file) && !env.file.is_directory)
					env.path = *file;
			}
			else
				env.path = *file;

			if (!env.file.is_exists)
			{
				VI_ERR("path <%s> does not exist", env.commandline.params.front().c_str());
				return (int)exit_status::input_error;
			}

			env.registry = os::path::get_directory(env.path);
			if (env.registry == env.path)
			{
				env.path = directory + env.path;
				env.registry = os::path::get_directory(env.path);
			}

			env.library = os::path::get_filename(env.path).data();
			env.program = *os::file::read_as_string(env.path);
			env.registry += "addons";
			env.registry += VI_SPLITTER;
		}

		if (!config.interactive && env.addon.empty() && env.program.empty())
		{
			config.interactive = true;
			if (env.commandline.args.size() > 1)
			{
				VI_ERR("provide a path to existing script file");
				return (int)exit_status::input_error;
			}
		}
		else if (!env.addon.empty())
		{
			if (builder::initialize_into_addon(config, env, vm, builder::get_default_settings()) != status_code::OK)
				return (int)exit_status::command_error;

			terminal->write_line("Initialized " + env.mode + " addon: " + env.addon);
			return (int)exit_status::OK;
		}

		unit = vm->create_compiler();
		if (!runtime::configure_context(config, env, vm, unit))
			return (int)exit_status::compiler_error;

		os::directory::set_working(os::path::get_directory(env.path.c_str()).c_str());
		if (config.debug)
		{
			debugger_context* debugger = new debugger_context();
			bindings::heavy_registry().bind_stringifiers(debugger);
			debugger->set_interrupt_callback([](bool is_interrupted)
			{
				if (!schedule::is_available())
					return;

				auto* queue = schedule::get();
				if (is_interrupted)
					queue->suspend();
				else
					queue->resume();
			});
			vm->set_debugger(debugger);
		}

		unit->set_include_callback(std::bind(&environment::import_addon, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		auto status = unit->prepare(env.library);
		if (!status)
		{
			VI_ERR("cannot prepare <%s> module scope\n  %s", env.library, status.error().what());
			return (int)exit_status::prepare_error;
		}

		context = vm->request_context();
		if (!env.program.empty())
		{
			if (!config.load_byte_code)
			{
				status = unit->load_code(env.path, env.program);
				if (!status)
				{
					VI_ERR("cannot load <%s> module script code\n  %s", env.library, status.error().what());
					return (int)exit_status::loading_error;
				}

				runtime::configure_system(config);
				status = unit->compile().get();
				if (!status)
				{
					VI_ERR("cannot compile <%s> module\n  %s", env.library, status.error().what());
					return (int)exit_status::compiler_error;
				}
			}
			else
			{
				byte_code_info info;
				info.data.insert(info.data.begin(), env.program.begin(), env.program.end());

				runtime::configure_system(config);
				status = unit->load_byte_code(&info).get();
				if (!status)
				{
					VI_ERR("cannot load <%s> module bytecode\n  %s", env.library, status.error().what());
					return (int)exit_status::loading_error;
				}
			}
		}

		if (config.install)
		{
			if (config.installed > 0)
			{
				terminal->write_line("Successfully installed " + to_string(config.installed) + string(config.installed > 1 ? " addons" : " addon"));
				return EXIT_SUCCESS;
			}
			else if (env.output.empty())
				return builder::pull_addon_repository(config, env) == status_code::OK ? (int)exit_status::OK : (int)exit_status::command_error;

			if (builder::compile_into_executable(config, env, vm, builder::get_default_settings()) != status_code::OK)
				return (int)exit_status::command_error;

			terminal->write_line("Built binaries directory: " + env.output + "bin");
			return (int)exit_status::OK;
		}
		else if (config.save_byte_code)
		{
			byte_code_info info;
			info.debug = config.debug;
			if (unit->save_byte_code(&info) && os::file::write(env.path + ".gz", (uint8_t*)info.data.data(), info.data.size()))
				return (int)exit_status::OK;

			VI_ERR("cannot save <%s> module bytecode", env.library);
			return (int)exit_status::saving_error;
		}
		else if (config.dependencies)
		{
			print_dependencies();
			return (int)exit_status::OK;
		}
		else if (config.interactive)
		{
			if (env.path.empty())
				env.path = *os::directory::get_working();

			string data, multidata;
			data.reserve(1024 * 1024);
			if (config.essentials_only)
			{
				vm->import_system_addon("any");
				vm->import_system_addon("uint256");
				vm->import_system_addon("math");
				vm->import_system_addon("random");
				vm->import_system_addon("timestamp");
				vm->import_system_addon("console");
			}
			else
				vm->import_system_addon("*");
			print_introduction("interactive mode");

			auto* debugger = new debugger_context(debug_type::detach);
			bindings::heavy_registry().bind_stringifiers(debugger);

			char default_code[] = "void main(){}";
			bool editor = false;
			size_t section = 0;
			env.path += env.library;
			debugger->set_engine(vm);

			function main = runtime::get_entrypoint(env, entrypoint, unit, true);
			if (!main.is_valid())
			{
				status = unit->load_code(env.path + ":0", default_code);
				if (!status)
				{
					VI_ERR("cannot load default entrypoint for interactive mode\n  %s", status.error().what());
					memory::release(debugger);
					return (int)exit_status::loading_error;
				}

				runtime::configure_system(config);
				status = unit->compile().get();
				if (!status)
				{
					VI_ERR("cannot compile default module for interactive mode\n  %s", status.error().what());
					memory::release(debugger);
					return (int)exit_status::compiler_error;
				}
			}

			for (;;)
			{
				if (!editor)
					terminal->write("> ");

				if (!terminal->read_line(data, data.capacity()))
				{
					if (!editor)
						break;

				exit_editor:
					editor = false;
					data = multidata;
					multidata.clear();
					goto execute;
				}
				else if (data.empty())
					continue;

				stringify::trim(data);
				if (editor)
				{
					if (!data.empty() && data.back() == '\x4')
					{
						data.erase(data.end() - 1);
						if (env.inlined && !data.empty() && data.back() != ';')
							data += ';';

						multidata += data;
						goto exit_editor;
					}

					if (env.inlined && !data.empty() && data.back() != ';')
						data += ';';

					multidata += data;
					continue;
				}
				else if (stringify::starts_with(data, ".help"))
				{
					terminal->write_line("  .mode   - switch between registering and executing the code");
					terminal->write_line("  .help   - show available commands");
					terminal->write_line("  .editor - enter editor mode");
					terminal->write_line("  .exit   - exit interactive mode");
					terminal->write_line("  .use    - import system addons by name (comma separated list)");
					terminal->write_line("  *       - anything else will be interpreted as script code");
					continue;
				}
				else if (stringify::starts_with(data, ".use"))
				{
					size_t imports = 0;
					auto addons = stringify::split(data.substr(4), ',');
					for (auto& addon : addons)
					{
						stringify::trim(addon);
						if (!vm->import_system_addon(addon.empty() ? "*" : addon))
							terminal->write_line("  use: addon @" + addon + " not found");
						else
							++imports;
					}
					if (imports > 0)
						print_introduction("interactive mode");
					continue;
				}
				else if (stringify::starts_with(data, ".mode"))
				{
					env.inlined = !env.inlined;
					if (env.inlined)
						terminal->write_line("  evaluation mode: you may now execute your code");
					else
						terminal->write_line("  register mode: you may now register script interfaces");
					continue;
				}
				else if (stringify::starts_with(data, ".editor"))
				{
					terminal->write_line("  editor mode: you may write multiple lines of code (ctrl+d to finish)\n");
					editor = true;
					continue;
				}
				else if (data == ".exit")
					break;

			execute:
				if (data.empty())
					continue;

				if (!env.inlined)
				{
					string index = ":" + to_string(++section);
					if (!unit->load_code(env.path + index, data) || unit->compile().get())
						continue;
				}

				auto inlined = unit->compile_function(data, "any@").get();
				if (!inlined)
					continue;

				bindings::any* value = nullptr;
				auto execution = context->execute_call(*inlined, nullptr).get();
				if (execution && *execution == execution::finished)
				{
					string indent = "  ";
					value = context->get_return_object<bindings::any>();
					terminal->write_line(indent + debugger->to_string(indent, 3, value, vm->get_type_info_by_name("any").get_type_id()));
				}
				else
					context->abort();
				context->unprepare();
			}

			memory::release(debugger);
			exit_process(exit_status::OK);
			return (int)exit_status::OK;
		}

		function main = runtime::get_entrypoint(env, entrypoint, unit);
		if (!main.is_valid())
			return (int)exit_status::entrypoint_error;

		if (config.debug)
			print_introduction("debugger");

		int exit_code = 0;
		auto type = vm->get_type_info_by_decl("array<string>@");
		bindings::array* args_array = type.is_valid() ? bindings::array::compose<string>(type.get_type_info(), env.commandline.params) : nullptr;
		vm->set_exception_callback([](immediate_context* context)
		{
			if (!context->will_exception_be_caught())
				exit_process(exit_status::runtime_error);
		});

		main.add_ref();
		loop = new event_loop();
		loop->listen(context);
		loop->enqueue(function_delegate(main, context), [&main, args_array](immediate_context* context)
		{
			runtime::startup_environment(environment_config::get());
			if (main.get_args_count() > 0)
				context->set_arg_object(0, args_array);
		}, [this, &exit_code, &type, &main, args_array](immediate_context* context)
		{
			exit_code = main.get_return_type_id() == (int)type_id::void_t ? 0 : (int)context->get_return_dword();
			if (args_array != nullptr)
				context->get_vm()->release_object(args_array, type);
			runtime::shutdown_environment(environment_config::get());
			loop->wakeup();
		});

		runtime::await_context(mutex, loop, vm, context);
		return exit_code;
	}
	void environment::shutdown(int value)
	{
		umutex<std::mutex> unique(mutex);
		{
			if (runtime::try_context_exit(env, value))
			{
				loop->wakeup();
				VI_DEBUG("graceful shutdown using [signal vcall]");
				goto graceful_shutdown;
			}

			auto* app = application::get();
			if (app != nullptr && app->get_state() == application_state::active)
			{
				app->stop();
				loop->wakeup();
				VI_DEBUG("graceful shutdown using [application stop]");
				goto graceful_shutdown;
			}

			if (schedule::is_available())
			{
				schedule::get()->stop();
				loop->wakeup();
				VI_DEBUG("graceful shutdown using [scheduler stop]");
				goto graceful_shutdown;
			}

			VI_DEBUG("forcing shutdown using [kill]");
			return exit_process(exit_status::kill);
		}
	graceful_shutdown:
		listen_for_signals();
	}
	void environment::interrupt(int value)
	{
		if (config.debug && vm->get_debugger() && vm->get_debugger()->interrupt())
			listen_for_signals();
		else
			shutdown(value);
	}
	void environment::abort(const char* signal)
	{
		VI_PANIC(false, "%s (critical runtime error)", signal);
	}
	size_t environment::get_init_flags()
	{
		size_t library_layer = vitex::use_networking | vitex::use_cryptography | vitex::use_locale;
		size_t application_layer = library_layer | vitex::use_providers;
		size_t game_layer = application_layer | vitex::use_platform | vitex::use_audio | vitex::use_graphics;
		if (config.install)
			return library_layer;

		if (config.essentials_only)
			return application_layer;

		return game_layer;
	}
	void environment::add_default_commands()
	{
		add_command("application", "-h, --help", "show help message", true, [this](const std::string_view&)
		{
			print_help();
			return (int)exit_status::OK;
		});
		add_command("application", "-v, --version", "show version message", true, [this](const std::string_view&)
		{
			print_introduction("runtime");
			return (int)exit_status::OK;
		});
		add_command("application", "--log-plain", "show detailed log messages as is", true, [this](const std::string_view&)
		{
			config.pretty_progress = false;
			error_handling::set_flag(log_option::pretty, false);
			return (int)exit_status::next;
		});
		add_command("application", "--log-quiet", "disable logging", true, [](const std::string_view&)
		{
			error_handling::set_flag(log_option::active, false);
			return (int)exit_status::next;
		});
		add_command("application", "--log-time", "append date for each logging message", true, [](const std::string_view&)
		{
			error_handling::set_flag(log_option::dated, true);
			return (int)exit_status::next;
		});
		add_command("execution", "--load-bytecode", "load gz compressed compiled bytecode and execute it as normal", true, [this](const std::string_view&)
		{
			config.load_byte_code = true;
			return (int)exit_status::next;
		});
		add_command("execution", "--save-bytecode", "save gz compressed compiled bytecode to a file near script file", true, [this](const std::string_view&)
		{
			config.save_byte_code = true;
			return (int)exit_status::next;
		});
		add_command("execution", "-i, --interactive", "run only in interactive mode", true, [this](const std::string_view&)
		{
			config.interactive = true;
			return (int)exit_status::next;
		});
		add_command("execution", "-d, --debug", "enable debugger interface", true, [this](const std::string_view&)
		{
			config.debug = true;
			return (int)exit_status::next;
		});
		add_command("execution", "-e, --engine", "enable game engine mode for graphics and audio support", true, [this](const std::string_view&)
		{
			config.essentials_only = false;
			return (int)exit_status::next;
		});
		add_command("execution", "-p, --preserve", "enable in memory source code preservation for better exception messages", true, [this](const std::string_view&)
		{
			config.save_source_code = true;
			return (int)exit_status::next;
		});
		add_command("execution", "-d, --deny", "deny permissions by name [expects: plus(+) separated list]", false, [this](const std::string_view& value)
		{
			for (auto& item : stringify::split(value, '+'))
			{
				auto option = os::control::to_option(item);
				if (!option)
				{
					VI_ERR("os access control option not found: %s (options = %s)", item.c_str(), os::control::to_options());
					return (int)exit_status::input_error;
				}

				config.permissions[*option] = false;
			}
			return (int)exit_status::next;
		});
		add_command("execution", "-a, --allow", "allow permissions by name [expects: plus(+) separated list]", false, [this](const std::string_view& value)
		{
			for (auto& item : stringify::split(value, '+'))
			{
				auto option = os::control::to_option(item);
				if (!option)
				{
					VI_ERR("os access control option not found: %s (options = %s)", item.c_str(), os::control::to_options());
					return (int)exit_status::input_error;
				}

				config.permissions[*option] = true;
			}
			return (int)exit_status::next;
		});
		add_command("building", "--target", "set a cmake name for output target [expects: name]", false, [this](const std::string_view& name)
		{
			env.name = name;
			return (int)exit_status::next;
		});
		add_command("building", "--output", "directory where to build an executable from source code [expects: path]", false, [this](const std::string_view& path)
		{
			file_entry file;
			if (path != ".")
			{
				auto target = os::path::resolve(path, *os::directory::get_working(), true);
				if (target)
					env.output = *target;
			}
			else
				env.output = *os::directory::get_working();

			if (!env.output.empty() && (env.output.back() == '/' || env.output.back() == '\\'))
				env.output.erase(env.output.end() - 1);

			if (execute_argument({ "target" }) == exit_status::invalid_command || env.name.empty())
			{
				env.name = os::path::get_filename(env.output.c_str());
				if (env.name.empty())
				{
					VI_ERR("init directory is set but name was not specified: use --target");
					return (int)exit_status::input_error;
				}
			}

			env.output += VI_SPLITTER + env.name + VI_SPLITTER;
			if (!os::file::get_state(env.output, &file))
			{
				os::directory::patch(env.output);
				return (int)exit_status::next;
			}

			if (file.is_directory)
				return (int)exit_status::next;

			VI_ERR("output path <%s> must be a directory", path.data());
			return (int)exit_status::input_error;
		});
		add_command("building", "--import", "import standard addon(s) by name [expects: plus(+) separated list]", false, [this](const std::string_view& value)
		{
			for (auto& item : stringify::split(value, '+'))
				config.system_addons.push_back(item);

			return (int)exit_status::next;
		});
		add_command("building", "--import-addon", "import user addon(s) by path [expects: plus(+) separated list]", false, [this](const std::string_view& value)
		{
			for (auto& item : stringify::split(value, '+'))
				config.libraries.emplace_back(std::make_pair(item, true));

			return (int)exit_status::next;
		});
		add_command("building", "--import-library", "import clibrary(ies) by path [expects: plus(+) separated list]", false, [this](const std::string_view& value)
		{
			for (auto& item : stringify::split(value, '+'))
				config.libraries.emplace_back(std::make_pair(item, false));

			return (int)exit_status::next;
		});
		add_command("building", "--import-function", "import clibrary function by declaration [expects: clib_name:cfunc_name=asfunc_decl]", false, [this](const std::string_view& value)
		{
			size_t offset1 = value.find(':');
			if (offset1 == std::string::npos)
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", value.data());
				return (int)exit_status::invalid_declaration;
			}

			size_t offset2 = value.find('=', offset1);
			if (offset2 == std::string::npos)
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", value.data());
				return (int)exit_status::invalid_declaration;
			}

			string clibrary_name = string(value.substr(0, offset1));
			stringify::trim(clibrary_name);

			string cfunction_name = string(value.substr(offset1 + 1, offset2 - offset1 - 1));
			stringify::trim(clibrary_name);

			string declaration = string(value.substr(offset2 + 1));
			stringify::trim(clibrary_name);

			if (clibrary_name.empty() || cfunction_name.empty() || declaration.empty())
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", value.data());
				return (int)exit_status::invalid_declaration;
			}

			auto& data = config.functions[clibrary_name];
			data.first = cfunction_name;
			data.second = declaration;
			return (int)exit_status::next;
		});
		add_command("building", "--set-prop", "set virtual machine property [expects: prop_name:prop_value]", false, [this](const std::string_view& value)
		{
			auto args = stringify::split(value, ':');
			if (args.size() != 2)
			{
				VI_ERR("invalid property declaration <%s>", value.data());
				return (int)exit_status::input_error;
			}

			auto it = settings.find(stringify::trim(args[0]));
			if (it == settings.end())
			{
				VI_ERR("invalid property name <%s>", args[0].c_str());
				return (int)exit_status::input_error;
			}

			string& data = args[1];
			stringify::trim(data);
			stringify::to_lower(data);

			if (data.empty())
			{
			input_failure:
				VI_ERR("property value <%s>: %s", args[0].c_str(), args[1].empty() ? "?" : args[1].c_str());
				return (int)exit_status::input_error;
			}
			else if (!stringify::has_integer(data))
			{
				if (args[1] == "on" || args[1] == "true")
					vm->set_property((features)it->second, 1);
				else if (args[1] == "off" || args[1] == "false")
					vm->set_property((features)it->second, 1);
				else
					goto input_failure;
			}

			vm->set_property((features)it->second, (size_t)*from_string<uint64_t>(data));
			return (int)exit_status::next;
		});
		add_command("building", "--view-props", "show virtual machine properties message", true, [this](const std::string_view&)
		{
			print_properties();
			return (int)exit_status::OK;
		});
		add_command("addons", "-a, --addon", "initialize an addon in given directory [expects: [native|vm]:?relpath]", false, [this](const std::string_view& value)
		{
			string path = string(value);
			size_t where = value.find(':');
			if (where != std::string::npos)
			{
				path = path.substr(where + 1);
				if (path.empty())
				{
					VI_ERR("addon initialization expects <mode:path> format: path must not be empty");
					return (int)exit_status::input_error;
				}

				env.mode = value.substr(0, where);
				if (env.mode != "native" && env.mode != "vm")
				{
					VI_ERR("addon initialization expects <mode:path> format: mode <%s> is invalid, [native|vm] expected", env.mode.c_str());
					return (int)exit_status::input_error;
				}
			}
			else
				env.mode = "vm";

			file_entry file;
			if (path != ".")
			{
				auto target = os::path::resolve(path, *os::directory::get_working(), true);
				if (target)
					env.addon = *target;
			}
			else
				env.addon = *os::directory::get_working();

			if (!env.addon.empty() && (env.addon.back() == '/' || env.addon.back() == '\\'))
				env.addon.erase(env.addon.end() - 1);

			if (execute_argument({ "target" }) == exit_status::invalid_command || env.name.empty())
			{
				env.name = os::path::get_filename(env.addon.c_str());
				if (env.name.empty())
				{
					VI_ERR("init directory is set but name was not specified: use --target");
					return (int)exit_status::input_error;
				}
			}

			env.addon += VI_SPLITTER + env.name + VI_SPLITTER;
			if (!os::file::get_state(env.addon, &file))
			{
				os::directory::patch(env.addon);
				return (int)exit_status::next;
			}

			if (file.is_directory)
				return (int)exit_status::next;

			VI_ERR("addon path <%s> must be a directory", path.c_str());
			return (int)exit_status::input_error;
		});
		add_command("addons", "-i, --install", "install or update script dependencies", true, [this](const std::string_view& value)
		{
			config.install = true;
			return (int)exit_status::next;
		});
		add_command("addons", "--addons", "install and show dependencies message", true, [this](const std::string_view&)
		{
			config.dependencies = true;
			return (int)exit_status::next;
		});
	}
	void environment::add_default_settings()
	{
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
		settings["array_template_message"] = (uint32_t)features::expand_def_array_to_impl;
		settings["automatic_gc"] = (uint32_t)features::auto_garbage_collect;
		settings["automatic_constructors"] = (uint32_t)features::always_impl_default_construct;
		settings["value_assignment_to_references"] = (uint32_t)features::disallow_value_assign_for_ref_type;
		settings["named_args_mode"] = (uint32_t)features::alter_syntax_named_args;
		settings["integer_division_mode"] = (uint32_t)features::disable_integer_division;
		settings["no_empty_list_elements"] = (uint32_t)features::disallow_empty_list_elements;
		settings["private_is_protected"] = (uint32_t)features::private_prop_as_protected;
		settings["heredoc_trim_mode"] = (uint32_t)features::heredoc_trim_mode;
		settings["generic_auto_handles_mode"] = (uint32_t)features::generic_call_mode;
		settings["ignore_shared_interface_duplicates"] = (uint32_t)features::ignore_duplicate_shared_int;
		settings["ignore_debug_output"] = (uint32_t)features::no_debug_output;
		settings["disable_script_class_gc"] = (uint32_t)features::disable_script_class_gc;
		settings["jit_interface_version"] = (uint32_t)features::jit_interface_version;
		settings["always_impl_default_copy"] = (uint32_t)features::always_impl_default_copy;
		settings["always_impl_default_copy_construct"] = (uint32_t)features::always_impl_default_copy_construct;
		settings["member_init_mode"] = (uint32_t)features::member_init_mode;
		settings["bool_conversion_mode"] = (uint32_t)features::bool_conversion_mode;
		settings["foreach_support"] = (uint32_t)features::foreach_support;
	}
	void environment::add_command(const std::string_view& category, const std::string_view& name, const std::string_view& description, bool is_flag_only, const command_callback& callback)
	{
		environment_command command;
		command.arguments = stringify::split(name, ',');
		command.description = description;
		command.callback = callback;

		for (auto& argument : command.arguments)
		{
			stringify::trim(argument);
			while (!argument.empty() && argument.front() == '-')
				argument.erase(argument.begin());
			if (is_flag_only)
				flags.insert(argument);
		}

		auto& target = commands[string(category)];
		target.push_back(std::move(command));
	}
	exit_status environment::execute_argument(const unordered_set<string>& names)
	{
		for (auto& next : env.commandline.args)
		{
			if (names.find(next.first) == names.end())
				continue;

			auto* command = find_argument(next.first);
			if (!command)
				return exit_status::invalid_command;

			int exit_code = command->callback(next.second);
			if (exit_code != (int)exit_status::next)
				return (exit_status)exit_code;
		}

		return exit_status::OK;
	}
	environment_command* environment::find_argument(const std::string_view& name)
	{
		for (auto& category : commands)
		{
			for (auto& command : category.second)
			{
				for (auto& argument : command.arguments)
				{
					if (argument == name)
						return &command;
				}
			}
		}

		return nullptr;
	}
	expects_preprocessor<include_type> environment::import_addon(preprocessor* base, const include_result& file, string& output)
	{
		if (file.library.empty() || file.library.front() != '@')
			return include_type::unchanged;

		if (!control::has(config, access_option::https))
		{
			VI_ERR("cannot import addon <%s> from remote repository: permission denied", file.library.c_str());
			return include_type::error;
		}

		include_type status;
		if (!builder::is_addon_target_exists(env, vm, file.library))
		{
			if (!config.install)
			{
				VI_ERR("program requires <%s> addon: run installation with --install flag", file.library.c_str());
				status = include_type::error;
			}
			else if (builder::compile_into_addon(config, env, vm, file.library, output) == status_code::OK)
			{
				status = output.empty() ? include_type::computed : include_type::preprocess;
				++config.installed;
			}
			else
				status = include_type::error;
		}
		else if (builder::import_into_addon(env, vm, file.library, output) == status_code::OK)
			status = output.empty() ? include_type::computed : include_type::preprocess;
		else
			status = include_type::error;
		env.addons.insert(file.library);
		return status;
	}
	void environment::print_introduction(const char* label)
	{
		auto* terminal = console::get();
		auto* lib = vitex::runtime::get();
		terminal->write("Welcome to asx ");
		terminal->write(label);
		terminal->write(" v");
		terminal->write(to_string((uint32_t)vitex::major_version));
		terminal->write(".");
		terminal->write(to_string((uint32_t)vitex::minor_version));
		terminal->write(".");
		terminal->write(to_string((uint32_t)vitex::patch_version));
		terminal->write(".");
		terminal->write(to_string((uint32_t)vitex::build_version));
		terminal->write(" [");
		terminal->write(lib->get_compiler());
		terminal->write(" ");
		terminal->write(lib->get_build());
		terminal->write(" on ");
		terminal->write(lib->get_platform());
		terminal->write("]\n");
		terminal->write("Run \"" + string(config.interactive ? ".help" : (config.debug ? "help" : "asx --help")) + "\" for more information");
		if (config.interactive)
			terminal->write(" (loaded " + to_string(vm->get_exposed_addons().size()) + " addons)");
		terminal->write("\n");
	}
	void environment::print_help()
	{
		size_t max = 0;
		for (auto& category : commands)
		{
			for (auto& next : category.second)
			{
				size_t size = 0;
				for (auto& argument : next.arguments)
					size += (argument.size() > 1 ? 2 : 1) + argument.size() + 2;
				size -= 2;
				if (size > max)
					max = size;
			}
		}

		auto* terminal = console::get();
		terminal->write_line("Usage: asx [options?...]");
		terminal->write_line("       asx [options?...] [file.as] [arguments?...]\n");
		for (auto& category : commands)
		{
			string name = category.first;
			terminal->write_line("Category: " + stringify::to_upper(name));
			for (auto& next : category.second)
			{
				string command;
				for (auto& argument : next.arguments)
					command += (argument.size() > 1 ? "--" : "-") + argument + ", ";

				command.erase(command.size() - 2, 2);
				size_t spaces = max - command.size();
				terminal->write("    ");
				terminal->write(command);
				for (size_t i = 0; i < spaces; i++)
					terminal->write(" ");
				terminal->write_line(" - " + next.description);
			}
			terminal->write_char('\n');
		}
	}
	void environment::print_properties()
	{
		auto* terminal = console::get();
		for (auto& item : settings)
		{
			size_t value = vm->get_property((features)item.second);
			terminal->write("  " + item.first + ": ");
			if (stringify::ends_with(item.first, "mode"))
				terminal->write("mode " + to_string(value));
			else if (stringify::ends_with(item.first, "size"))
				terminal->write((value > 0 ? to_string(value) : "unlimited"));
			else if (value == 0)
				terminal->write("OFF");
			else if (value == 1)
				terminal->write("ON");
			else
				terminal->write(to_string(value));
			terminal->write("\n");
		}
	}
	void environment::print_dependencies()
	{
		auto* terminal = console::get();
		auto exposes = vm->get_exposed_addons();
		if (!exposes.empty())
		{
			terminal->write_line("  local dependencies list:");
			for (auto& item : exposes)
				terminal->write_line("    " + stringify::replace(item, ":", ": "));
		}

		if (!env.addons.empty())
		{
			terminal->write_line("  remote dependencies list:");
			for (auto& item : env.addons)
				terminal->write_line("    " + item + ": " + builder::get_addon_target_library(env, vm, item, nullptr));
		}
	}
	void environment::listen_for_signals()
	{
		static environment* instance = this;
		signal(SIGINT, [](int value) { instance->interrupt(value); });
		signal(SIGTERM, [](int value) { instance->shutdown(value); });
		signal(SIGFPE, [](int) { instance->abort("division by zero"); });
		signal(SIGILL, [](int) { instance->abort("illegal instruction"); });
		signal(SIGSEGV, [](int) { instance->abort("segmentation fault"); });
#ifdef VI_UNIX
		signal(SIGPIPE, SIG_IGN);
		signal(SIGCHLD, SIG_IGN);
#endif
	}
	void environment::exit_process(exit_status code)
	{
		if (code != exit_status::runtime_error)
			os::process::exit((int)code);
		else
			os::process::abort();
	}
}

int main(int argc, char* argv[])
{
	auto* instance = new asx::environment(argc, argv);
	vitex::heavy_runtime scope(instance->get_init_flags());
	int exit_code = instance->dispatch();
	delete instance;
	return exit_code;
}