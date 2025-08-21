#ifndef RUNTIME_H
#define RUNTIME_H
#include <vengeance/bindings.h>
#include <vengeance/vengeance.h>

using namespace vitex::core;
using namespace vitex::compute;
using namespace vitex::scripting;

namespace asx
{
	enum class exit_status
	{
		next = 0x00fffff - 1,
		ok = 0,
		runtime_error,
		prepare_error,
		loading_error,
		saving_error,
		compiler_error,
		entrypoint_error,
		input_error,
		invalid_command,
		invalid_declaration,
		command_error,
		kill
	};

	struct program_entrypoint
	{
		const char* returns_with_args = "int main(array<string>@)";
		const char* returns = "int main()";
		const char* simple = "void main()";
	};

	struct environment_config
	{
		inline_args commandline;
		unordered_set<string> addons;
		function_delegate at_exit;
		file_entry file;
		string name;
		string path;
		string program;
		string registry;
		string mode;
		string output;
		string addon;
		compiler* this_compiler;
		const char* library;
		int32_t auto_schedule;
		bool auto_console;
		bool auto_stop;
		bool inlined;

		environment_config() : this_compiler(nullptr), library("__anonymous__"), auto_schedule(-1), auto_console(false), auto_stop(false), inlined(true)
		{
		}
		void parse(int args_count, char** args_data, const unordered_set<string>& flags = { })
		{
			commandline = os::process::parse_args(args_count, args_data, (size_t)args_format::key_value | (size_t)args_format::flag_value | (size_t)args_format::stop_if_no_match, flags);
		}
		static environment_config& get(environment_config* other = nullptr)
		{
			static environment_config* base = other;
			VI_ASSERT(base != nullptr, "env was not set");
			return *base;
		}
	};

	struct system_config
	{
		unordered_map<string, std::pair<string, string>> functions;
		unordered_map<access_option, bool> permissions;
		vector<std::pair<string, bool>> libraries;
		vector<std::pair<string, int32_t>> settings;
		vector<string> system_addons;
		bool ts_imports = true;
		bool tags = true;
		bool debug = false;
		bool interactive = false;
		bool essentials_only = true;
		bool load_byte_code = false;
		bool save_byte_code = false;
		bool save_source_code = false;
		bool full_stack_tracing = true;
		bool dependencies = false;
		bool install = false;
		size_t installed = 0;
	};

	class runtime
	{
	public:
		static void startup_environment(environment_config& env)
		{
			if (env.auto_schedule >= 0)
				schedule::get()->start(env.auto_schedule > 0 ? schedule::desc((size_t)env.auto_schedule) : schedule::desc());

			if (env.auto_console)
				console::get()->attach();
		}
		static void shutdown_environment(environment_config& env)
		{
			if (env.auto_stop)
				schedule::get()->stop();
		}
		static void configure_system(system_config& config)
		{
			for (auto& option : config.permissions)
				os::control::set(option.first, option.second);
		}
		static bool configure_context(system_config& config, environment_config& env, virtual_machine* vm, compiler* this_compiler)
		{
			vm->set_ts_imports(config.ts_imports);
			vm->set_module_directory(os::path::get_directory(env.path.c_str()));
			vm->set_preserve_source_code(config.save_source_code);
			vm->set_full_stack_tracing(config.full_stack_tracing);

			for (auto& name : config.system_addons)
			{
				if (!vm->import_system_addon(name))
				{
					VI_ERR("%s import error: not found", name.c_str());
					return false;
				}
			}

			for (auto& path : config.libraries)
			{
				if (!vm->import_clibrary(path.first, path.second))
				{
					VI_ERR("%s import error: %s not found", path.second ? "addon" : "clibrary", path.first.c_str());
					return false;
				}
			}

			for (auto& data : config.functions)
			{
				if (!vm->import_cfunction({ data.first }, data.second.first, data.second.second))
				{
					VI_ERR("%s import error: %s not found", data.second.first.c_str(), data.first.c_str());
					return false;
				}
			}

			auto* macro = this_compiler->get_processor();
			macro->add_default_definitions();

			env.this_compiler = this_compiler;
			bindings::tags::bind_syntax(vm, config.tags, &runtime::process_tags);
			environment_config::get(&env);

			vm->import_system_addon("ctypes");
			vm->begin_namespace("this_process");
			vm->set_function_def("void exit_event(int)");
			vm->set_function("void before_exit(exit_event@)", &runtime::apply_context_exit);
			vm->set_function("uptr@ get_compiler()", &runtime::get_compiler);
			vm->end_namespace();
			return true;
		}
		static bool try_context_exit(environment_config& env, int value)
		{
			if (!env.at_exit.is_valid())
				return false;

			auto status = env.at_exit([value](immediate_context* context)
			{
				context->set_arg32(0, value);
			}).get();
			env.at_exit.release();
			virtual_machine::cleanup_this_thread();
			return !!status;
		}
		static void apply_context_exit(asIScriptFunction* callback)
		{
			auto& env = environment_config::get();
			uptr<immediate_context> context = callback ? env.this_compiler->get_vm()->request_context() : nullptr;
			env.at_exit = function_delegate(callback, *context);
		}
		static void await_context(std::mutex& mutex, event_loop* loop, virtual_machine* vm, immediate_context* context)
		{
			event_loop::set(loop);
			while (loop->poll_extended(context, 1000))
			{
				vm->perform_periodic_garbage_collection(60000);
				loop->dequeue(vm);
			}

			umutex<std::mutex> unique(mutex);
			if (schedule::has_instance())
			{
				auto* queue = schedule::get();
				while (!queue->can_enqueue() && queue->has_any_tasks())
					queue->dispatch();
				schedule::cleanup_instance();
			}

			event_loop::set(nullptr);
			context->reset();
			vm->perform_full_garbage_collection();
			apply_context_exit(nullptr);
		}
		static void context_thrown(immediate_context* context)
		{
			if (context->will_exception_be_caught())
				return;

			auto exception = bindings::exception::pointer();
			exception.load_exception_data(context->get_exception_string());
			exception.context = context;

			auto& type = exception.get_type();
			auto& text = exception.get_text();
			VI_PANIC(false, "%s - %s", type.empty() ? "unknown_error" : type.c_str(), text.empty() ? "no description available" : text.c_str());
		}
		static function get_entrypoint(environment_config& env, program_entrypoint& entrypoint, compiler* unit, bool silent = false)
		{
			function main_returns_with_args = unit->get_module().get_function_by_decl(entrypoint.returns_with_args);
			function main_returns = unit->get_module().get_function_by_decl(entrypoint.returns);
			function main_simple = unit->get_module().get_function_by_decl(entrypoint.simple);
			if (main_returns_with_args.is_valid() || main_returns.is_valid() || main_simple.is_valid())
				return main_returns_with_args.is_valid() ? main_returns_with_args : (main_returns.is_valid() ? main_returns : main_simple);

			if (!silent)
				VI_ERR("%s module error: function \"%s\", \"%s\" or \"%s\" must be present", env.library, entrypoint.returns_with_args, entrypoint.returns, entrypoint.simple);
			return function(nullptr);
		}
		static compiler* get_compiler()
		{
			return environment_config::get().this_compiler;
		}

	private:
		static void process_tags(virtual_machine* vm, bindings::tags::tag_info&& info)
		{
			auto& env = environment_config::get();
			for (auto& tag : info)
			{
				if (tag.name != "main")
					continue;

				for (auto& directive : tag.directives)
				{
					if (directive.name == "#schedule::main")
					{
						auto threads = directive.args.find("threads");
						if (threads != directive.args.end())
							env.auto_schedule = from_string<uint8_t>(threads->second).or_else(0);
						else
							env.auto_schedule = 0;

						auto stop = directive.args.find("stop");
						if (stop != directive.args.end())
						{
							stringify::to_lower(threads->second);
							auto value = from_string<uint8_t>(threads->second);
							if (!value)
								env.auto_stop = (threads->second == "on" || threads->second == "true" || threads->second == "yes");
							else
								env.auto_stop = *value > 0;
						}
					}
					else if (directive.name == "#console::main")
						env.auto_console = true;
				}
			}
		}
	};
}
#endif
