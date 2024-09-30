#include "program.hpp"
#include "runtime.hpp"
#include <vengeance/vengeance.h>
#include <vengeance/bindings.h>
#include <vengeance/layer.h>
#include <signal.h>

using namespace Vitex::Layer;
using namespace ASX;

EventLoop* Loop = nullptr;
VirtualMachine* VM = nullptr;
Compiler* Unit = nullptr;
ImmediateContext* Context = nullptr;
std::mutex Mutex;
int ExitCode = 0;

void exit_program(int sigv)
{
	if (sigv != SIGINT && sigv != SIGTERM)
        return;

	UMutex<std::mutex> Unique(Mutex);
    {
        if (Runtime::TryContextExit(EnvironmentConfig::Get(), sigv))
        {
			Loop->Wakeup();
            goto GracefulShutdown;
        }

        auto* App = Application::Get();
        if (App != nullptr && App->GetState() == ApplicationState::Active)
        {
            App->Stop();
			Loop->Wakeup();
            goto GracefulShutdown;
        }

        if (Schedule::IsAvailable())
        {
            Schedule::Get()->Stop();
			Loop->Wakeup();
            goto GracefulShutdown;
        }

        return std::exit((int)ExitStatus::Kill);
    }
GracefulShutdown:
    signal(sigv, &exit_program);
}
void setup_program(EnvironmentConfig& Env)
{
    OS::Directory::SetWorking(Env.Path.c_str());
    signal(SIGINT, &exit_program);
    signal(SIGTERM, &exit_program);
#ifdef VI_UNIX
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
#endif
}
bool load_program(EnvironmentConfig& Env)
{
#ifdef HAS_PROGRAM_BYTECODE
    program_bytecode::foreach(&Env, [](void* Context, const char* Buffer, unsigned Size)
    {
        EnvironmentConfig* Env = (EnvironmentConfig*)Context;
	    Env->Program = Codec::Base64Decode(std::string_view(Buffer, (size_t)Size));
    });
    return true;
#else
    return false;
#endif
}
int main(int argc, char* argv[])
{
	EnvironmentConfig Env;
	Env.Path = *OS::Directory::GetModule();
	Env.Module = argc > 0 ? argv[0] : "runtime";
	Env.AutoSchedule = {{BUILDER_ENV_AUTO_SCHEDULE}};
	Env.AutoConsole = {{BUILDER_ENV_AUTO_CONSOLE}};
	Env.AutoStop = {{BUILDER_ENV_AUTO_STOP}};
    if (!load_program(Env))
        return 0;

	Vector<String> Args;
	Args.reserve((size_t)argc);
	for (int i = 0; i < argc; i++)
		Args.push_back(argv[i]);

	SystemConfig Config;
	Config.Permissions = { {{BUILDER_CONFIG_PERMISSIONS}} };
	Config.Libraries = { {{BUILDER_CONFIG_LIBRARIES}} };
	Config.Functions = { {{BUILDER_CONFIG_FUNCTIONS}} };
	Config.SystemAddons = { {{BUILDER_CONFIG_ADDONS}} };
	Config.Tags = {{BUILDER_CONFIG_TAGS}};
	Config.TsImports = {{BUILDER_CONFIG_TS_IMPORTS}};
	Config.EssentialsOnly = {{BUILDER_CONFIG_ESSENTIALS_ONLY}};
    setup_program(Env);

	size_t Modules = Vitex::LOAD_NETWORKING | Vitex::LOAD_CRYPTOGRAPHY | Vitex::LOAD_PROVIDERS | Vitex::LOAD_LOCALE;
	if (!Config.EssentialsOnly)
		Modules |= Vitex::LOAD_PLATFORM | Vitex::LOAD_AUDIO | Vitex::LOAD_GRAPHICS;

	Vitex::HeavyRuntime Scope(Modules);
	{
		VM = new VirtualMachine();
		Bindings::HeavyRegistry().BindAddons(VM);
		Unit = VM->CreateCompiler();
        Context = VM->RequestContext();
		
        Vector<std::pair<uint32_t, size_t>> Settings = { {{BUILDER_CONFIG_SETTINGS}} };
        for (auto& Item : Settings)
            VM->SetProperty((Features)Item.first, Item.second);

		Unit = VM->CreateCompiler();
		ExitCode = Runtime::ConfigureContext(Config, Env, VM, Unit) ? (int)ExitStatus::OK : (int)ExitStatus::CompilerError;
		if (ExitCode != (int)ExitStatus::OK)
			goto FinishProgram;

		Runtime::ConfigureSystem(Config);
		if (!Unit->Prepare(Env.Module))
		{
			VI_ERR("cannot prepare <%s> module scope", Env.Module);
			ExitCode = (int)ExitStatus::PrepareError;
			goto FinishProgram;
		}

		ByteCodeInfo Info;
		Info.Data.insert(Info.Data.begin(), Env.Program.begin(), Env.Program.end());
		if (!Unit->LoadByteCode(&Info).Get())
		{
			VI_ERR("cannot load <%s> module bytecode", Env.Module);
			ExitCode = (int)ExitStatus::LoadingError;
			goto FinishProgram;
		}

	    ProgramEntrypoint Entrypoint;
		Function Main = Runtime::GetEntrypoint(Env, Entrypoint, Unit);
		if (!Main.IsValid())
        {
			ExitCode = (int)ExitStatus::EntrypointError;
			goto FinishProgram;
        }

		int ExitCode = 0;
		TypeInfo Type = VM->GetTypeInfoByDecl("array<string>@");
		Bindings::Array* ArgsArray = Type.IsValid() ? Bindings::Array::Compose<String>(Type.GetTypeInfo(), Args) : nullptr;
		VM->SetExceptionCallback([](ImmediateContext* Context)
		{
			if (!Context->WillExceptionBeCaught())
				std::exit((int)ExitStatus::RuntimeError);
		});

		Main.AddRef();
		Loop = new EventLoop();
		Loop->Listen(Context);
		Loop->Enqueue(FunctionDelegate(Main, Context), [&Main, ArgsArray](ImmediateContext* Context)
		{
			Runtime::StartupEnvironment(EnvironmentConfig::Get());
			if (Main.GetArgsCount() > 0)
				Context->SetArgObject(0, ArgsArray);
		}, [&ExitCode, &Type, &Main, ArgsArray](ImmediateContext* Context)
		{
			ExitCode = Main.GetReturnTypeId() == (int)TypeId::VOIDF ? 0 : (int)Context->GetReturnDWord();
			if (ArgsArray != nullptr)
				Context->GetVM()->ReleaseObject(ArgsArray, Type);
			Runtime::ShutdownEnvironment(EnvironmentConfig::Get());
			Loop->Wakeup();
		});
        
		Runtime::AwaitContext(Mutex, Loop, VM, Context);
	}
FinishProgram:
	Memory::Release(Context);
	Memory::Release(Unit);
	Memory::Release(VM);
    Memory::Release(Loop);
	return ExitCode;
}