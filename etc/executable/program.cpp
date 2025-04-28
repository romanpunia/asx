#include "program.hpp"
#include "runtime.hpp"
#include <vengeance/vengeance.h>
#include <vengeance/bindings.h>
#include <vengeance/layer.h>
#include <signal.h>

using namespace vitex::layer;
using namespace asx;

event_loop* loop = nullptr;
virtual_machine* vm = nullptr;
compiler* unit = nullptr;
immediate_context* context = nullptr;
std::mutex mutex;
int exit_code = 0;

void exit_program(int sigv)
{
	if (sigv != SIGINT && sigv != SIGTERM)
		return;

	umutex<std::mutex> unique(mutex);
	{
		if (runtime::try_context_exit(environment_config::get(), sigv))
		{
			loop->wakeup();
			goto graceful_shutdown;
		}

		auto* app = application::get();
		if (app != nullptr && app->get_state() == application_state::active)
		{
			app->stop();
			loop->wakeup();
			goto graceful_shutdown;
		}

		if (schedule::is_available())
		{
			schedule::get()->stop();
			loop->wakeup();
			goto graceful_shutdown;
		}

		return std::exit((int)exit_status::kill);
	}
graceful_shutdown:
	signal(sigv, &exit_program);
}
void setup_program(environment_config& env)
{
	os::directory::set_working(env.path.c_str());
	signal(SIGINT, &exit_program);
	signal(SIGTERM, &exit_program);
#ifdef VI_UNIX
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#endif
}
bool load_program(environment_config& env)
{
#ifdef HAS_PROGRAM_BYTECODE
	program_bytecode::foreach(&env, [](void* context, const char* buffer, unsigned size)
	{
		environment_config* env = (environment_config*)context;
		env->program = codec::base64_decode(std::string_view(buffer, (size_t)size));
	});
	return true;
#else
	return false;
#endif
}
int main(int argc, char* argv[])
{
	environment_config env;
	env.path = *os::directory::get_module();
	env.library = argc > 0 ? argv[0] : "runtime";
	env.auto_schedule = {{BUILDER_ENV_AUTO_SCHEDULE}};
	env.auto_console = {{BUILDER_ENV_AUTO_CONSOLE}};
	env.auto_stop = {{BUILDER_ENV_AUTO_STOP}};
	if (!load_program(env))
		return 0;

	vector<string> args;
	args.reserve((size_t)argc);
	for (int i = 0; i < argc; i++)
		args.push_back(argv[i]);

	system_config config;
	config.permissions = { {{BUILDER_CONFIG_PERMISSIONS}} };
	config.libraries = { {{BUILDER_CONFIG_LIBRARIES}} };
	config.functions = { {{BUILDER_CONFIG_FUNCTIONS}} };
	config.system_addons = { {{BUILDER_CONFIG_ADDONS}} };
	config.tags = {{BUILDER_CONFIG_TAGS}};
	config.ts_imports = {{BUILDER_CONFIG_TS_IMPORTS}};
	config.essentials_only = {{BUILDER_CONFIG_ESSENTIALS_ONLY}};
	setup_program(env);

	size_t modules = vitex::use_networking | vitex::use_cryptography | vitex::use_providers | vitex::use_locale;
	if (!config.essentials_only)
		modules |= vitex::use_platform | vitex::use_audio | vitex::use_graphics;

	vitex::heavy_runtime scope(modules);
	{
		vm = new virtual_machine();
		bindings::heavy_registry().bind_addons(vm);
		unit = vm->create_compiler();
		context = vm->request_context();

		vector<std::pair<uint32_t, size_t>> settings = { {{BUILDER_CONFIG_SETTINGS}} };
		for (auto& item : settings)
			vm->set_property((features)item.first, item.second);

		unit = vm->create_compiler();
		exit_code = runtime::configure_context(config, env, vm, unit) ? (int)exit_status::OK : (int)exit_status::compiler_error;
		if (exit_code != (int)exit_status::OK)
			goto finish_program;

		runtime::configure_system(config);
		if (!unit->prepare(env.library))
		{
			VI_ERR("cannot prepare <%s> module scope", env.library);
			exit_code = (int)exit_status::prepare_error;
			goto finish_program;
		}

		byte_code_info info;
		info.data.insert(info.data.begin(), env.program.begin(), env.program.end());
		if (!unit->load_byte_code(&info).get())
		{
			VI_ERR("cannot load <%s> module bytecode", env.library);
			exit_code = (int)exit_status::loading_error;
			goto finish_program;
		}

		program_entrypoint entrypoint;
		function main = runtime::get_entrypoint(env, entrypoint, unit);
		if (!main.is_valid())
		{
			exit_code = (int)exit_status::entrypoint_error;
			goto finish_program;
		}

		int exit_code = 0;
		auto type = vm->get_type_info_by_decl("array<string>@");
		bindings::array* args_array = type.is_valid() ? bindings::array::compose<string>(type.get_type_info(), args) : nullptr;
		vm->set_exception_callback([](immediate_context* context)
		{
			if (!context->will_exception_be_caught())
				std::exit((int)exit_status::runtime_error);
		});

		main.add_ref();
		loop = new event_loop();
		loop->listen(context);
		loop->enqueue(function_delegate(main, context), [&main, args_array](immediate_context* context)
		{
			runtime::startup_environment(environment_config::get());
			if (main.get_args_count() > 0)
				context->set_arg_object(0, args_array);
		}, [&exit_code, &type, &main, args_array](immediate_context* context)
		{
			exit_code = main.get_return_type_id() == (int)type_id::void_t ? 0 : (int)context->get_return_dword();
			if (args_array != nullptr)
				context->get_vm()->release_object(args_array, type);
			runtime::shutdown_environment(environment_config::get());
			loop->wakeup();
		});

		runtime::await_context(mutex, loop, vm, context);
	}
finish_program:
	memory::release(context);
	memory::release(unit);
	memory::release(vm);
	memory::release(loop);
	return exit_code;
}