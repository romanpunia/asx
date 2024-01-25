#include "program.hpp"
#include "runtime.hpp"
#include <vitex/vitex.h>
#include <vitex/core/bindings.h>
#include <vitex/core/engine.h>
#include <signal.h>

using namespace Vitex::Engine;
using namespace ASX;

EventLoop* Loop = nullptr;
VirtualMachine* VM = nullptr;
Compiler* Unit = nullptr;
ImmediateContext* Context = nullptr;
Schedule* Queue = nullptr;
int ExitCode = 0;

void exit_program(int sigv)
{
	if (sigv != SIGINT && sigv != SIGTERM)
        return;
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

        auto* Queue = Schedule::Get();
        if (Queue->IsActive())
        {
            Queue->Stop();
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
	    Env->Program = Codec::Base64Decode((const unsigned char*)Buffer, (size_t)Size);
    });
    return true;
#else
    return false;
#endif
}
int main(int argc, char* argv[])
{
	EnvironmentConfig Env(argc, argv);
	Env.Path = *OS::Directory::GetModule();
	Env.Module = argc > 0 ? argv[0] : "runtime";
    if (!load_program(Env))
        return 0;

	SystemConfig Config;
	Config.Libraries = { {{BUILDER_CONFIG_LIBRARIES}} };
	Config.Functions = { {{BUILDER_CONFIG_FUNCTIONS}} };
	Config.SystemAddons = { {{BUILDER_CONFIG_ADDONS}} };
	Config.CLibraries = {{BUILDER_CONFIG_CLIBRARIES}};
	Config.CFunctions = {{BUILDER_CONFIG_CFUNCTIONS}};
	Config.TsImports = {{BUILDER_CONFIG_TS_IMPORTS}};
	Config.Addons = {{BUILDER_CONFIG_SYSTEM_ADDONS}};
	Config.Files = {{BUILDER_CONFIG_FILES}};
	Config.Remotes = {{BUILDER_CONFIG_REMOTES}};
	Config.Translator = {{BUILDER_CONFIG_TRANSLATOR}};
	Config.EssentialsOnly = {{BUILDER_CONFIG_ESSENTIALS_ONLY}};
    setup_program(Env);

	Vitex::Runtime Scope(Config.EssentialsOnly ? (size_t)Vitex::Preset::App : (size_t)Vitex::Preset::Game);
	{
		VM = new VirtualMachine();
		Unit = VM->CreateCompiler();
        Context = VM->RequestContext();
		Queue = Schedule::Get();
		Queue->SetImmediate(true);
		
        Vector<std::pair<uint32_t, size_t>> Settings = { {{BUILDER_CONFIG_SETTINGS}} };
        for (auto& Item : Settings)
            VM->SetProperty((Features)Item.first, Item.second);

		Unit = VM->CreateCompiler();
		ExitCode = Runtime::ConfigureContext(Config, Env, VM, Unit) ? (int)ExitStatus::OK : (int)ExitStatus::CompilerError;
		if (ExitCode != (int)ExitStatus::OK)
			goto FinishProgram;

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
		Bindings::Array* ArgsArray = Type.IsValid() ? Bindings::Array::Compose<String>(Type.GetTypeInfo(), Env.Args) : nullptr;
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
			if (Main.GetArgsCount() > 0)
				Context->SetArgObject(0, ArgsArray);
		}, [&ExitCode, &Type, &Main, ArgsArray](ImmediateContext* Context)
		{
			ExitCode = Main.GetReturnTypeId() == (int)TypeId::VOIDF ? 0 : (int)Context->GetReturnDWord();
			if (ArgsArray != nullptr)
				Context->GetVM()->ReleaseObject(ArgsArray, Type);
		});
        
		Runtime::AwaitContext(Queue, Loop, VM, Context);
	}
FinishProgram:
	VI_RELEASE(Context);
	VI_RELEASE(Unit);
	VI_RELEASE(VM);
    VI_RELEASE(Loop);
	VI_RELEASE(Queue);
	return ExitCode;
}