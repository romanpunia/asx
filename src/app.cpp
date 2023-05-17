#include <mavi/mavi.h>
#include <mavi/core/scripting.h>
#include <mavi/core/bindings.h>
#include <mavi/core/network.h>
#include <signal.h>
#define JUMP_CODE 1000
#define EXIT_SUCCESS 0
#define EXIT_RUNTIME_FAILURE 1
#define EXIT_PREPARE_FAILURE 2
#define EXIT_LOADING_FAILURE 3
#define EXIT_COMPILER_FAILURE 4
#define EXIT_ENTRYPOINT_FAILURE 5
#define EXIT_INPUT_FAILURE 6

using namespace Mavi::Core;
using namespace Mavi::Compute;
using namespace Mavi::Engine;
using namespace Mavi::Network;
using namespace Mavi::Scripting;

static VirtualMachine* VM = nullptr;
static Mavi::Scripting::Compiler* Unit = nullptr;
static const char* Entrypoint1 = "int main(array<string>@)";
static const char* Entrypoint2 = "int main()";
static const char* Entrypoint3 = "void main()";
static bool AttachDebugger = false;
int Abort(const char* Signal, bool Normal, bool Trace);

int CatchAll()
{
#ifdef VI_UNIX
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#endif
	signal(SIGABRT, [](int) { Abort("abort", false, true); });
	signal(SIGFPE, [](int) { Abort("division by zero", false, true); });
	signal(SIGILL, [](int) { Abort("illegal instruction", false, true); });
	signal(SIGINT, [](int) { Abort("interrupt", true, true); });
	signal(SIGSEGV, [](int) { Abort("segmentation fault", false, true); });
	signal(SIGTERM, [](int) { Abort("termination", true, true); });
	return 0;
}
int Abort(const char* Signal, bool Normal, bool Trace)
{
	if (AttachDebugger)
	{
		VI_WARN("[mavi] debugger mode, script context killed");
		std::exit(JUMP_CODE + EXIT_SUCCESS);
		return 0;
	}

	if (Normal)
	{
		static size_t Exits = 0; auto* App = Application::Get();
		if (!App || App->GetState() != ApplicationState::Active)
		{
			auto* Queue = Schedule::Get();
			if (!Queue->IsActive())
			{
				if (Exits > 0)
					VI_DEBUG("[mavi] script context is not responding; killed");

				std::exit(0);
			}
			else
			{
				++Exits;
				CatchAll();
				Queue->Stop();
			}
		}
		else
		{
			++Exits;
			CatchAll();
			App->Stop();
		}

		return JUMP_CODE + EXIT_SUCCESS;
	}
	else
	{
		if (Trace)
		{
			String StackTrace;
			ImmediateContext* Context = ImmediateContext::Get();
			if (Context != nullptr)
				StackTrace = Context->Get()->GetStackTrace(0, 64);
			else
				StackTrace = OS::GetStackTrace(0, 32);

			VI_ERR("[mavi] runtime error detected: %s; %s", Signal, StackTrace.c_str());
		}
		else
			VI_ERR("[mavi] runtime error detected: %s", Signal);

		std::exit(JUMP_CODE + EXIT_RUNTIME_FAILURE);
		return JUMP_CODE + EXIT_RUNTIME_FAILURE;
	}
}
int Execute(const String& Path, const String& Data, const Vector<String>& Args)
{
	auto* ModuleName = OS::Path::GetFilename(Path.c_str());
	auto* Queue = Schedule::Get();
	Queue->SetImmediate(true);

	VM = new VirtualMachine();
	VM->SetImports((uint32_t)Imports::All);
	VM->SetDocumentRoot(OS::Path::GetDirectory(Path.c_str()));

	if (AttachDebugger)
	{
		VI_WARN("[mavi] note script is running under context debugger");
		VM->SetDebugger(new DebuggerContext());
	}

	Unit = VM->CreateCompiler();
	if (Unit->Prepare(ModuleName, true) < 0)
	{
		VI_ERR("[mavi] cannot prepare <%s> module scope", ModuleName);
		return JUMP_CODE + EXIT_PREPARE_FAILURE;
	}

	if (Unit->LoadCode(Path, Data.c_str(), Data.size()) < 0)
	{
		VI_ERR("[mavi] cannot load <%s> module script code", ModuleName);
		return JUMP_CODE + EXIT_LOADING_FAILURE;
	}

	if (Unit->Compile().Get() < 0)
	{
		VI_ERR("[mavi] cannot compile <%s> module", ModuleName);
		return JUMP_CODE + EXIT_COMPILER_FAILURE;
	}

	Function Main1 = Unit->GetModule().GetFunctionByDecl(Entrypoint1);
	Function Main2 = Unit->GetModule().GetFunctionByDecl(Entrypoint2);
	Function Main3 = Unit->GetModule().GetFunctionByDecl(Entrypoint3);
	if (!Main1.IsValid() && !Main2.IsValid() && !Main3.IsValid())
	{
		VI_ERR("[mavi] module %s must contain either: <%s>, <%s> or <%s>", ModuleName, Entrypoint1, Entrypoint2, Entrypoint3);
		return JUMP_CODE + EXIT_ENTRYPOINT_FAILURE;
	}

	ImmediateContext* Context = Unit->GetContext();
	Context->SetExceptionCallback([](ImmediateContext* Context)
	{
		if (!Context->WillExceptionBeCaught())
			Abort(Context->GetExceptionString(), false, false);
	});

	TypeInfo Type = VM->GetTypeInfoByDecl("array<string>@");
	Bindings::Array* Params = Bindings::Array::Compose<String>(Type.GetTypeInfo(), Args);
	Function Entrypoint = Main1.IsValid() ? Main1 : (Main2.IsValid() ? Main2 : Main3);
	Context->Execute(Entrypoint, [&Entrypoint, Params](ImmediateContext* Context)
	{
		if (Entrypoint.GetArgsCount() > 0)
			Context->SetArgObject(0, Params);
	}).Wait();

	int ExitCode = !Main1.IsValid() && !Main2.IsValid() && Main3.IsValid() ? 0 : (int)Context->GetReturnDWord();
	VM->ReleaseObject(Params, Type);

	while (Queue->IsActive() || Context->GetState() == Activation::ACTIVE || Context->IsPending())
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	while (!Queue->CanEnqueue() && Queue->HasAnyTasks())
		Queue->Dispatch();

	VM->Collect();
	return ExitCode;
}
int Dispatch(char** ArgsData, int ArgsCount)
{
	auto* Output = Console::Get();
	Output->Show();

	Vector<String> Args;
	Args.reserve((size_t)ArgsCount);
	for (int i = 0; i < ArgsCount; i++)
		Args.push_back(ArgsData[i]);

	OS::Process::ArgsContext Params(ArgsCount, ArgsData);
	OS::SetLogPretty(!Params.Has("plain"));
	OS::SetLogActive(!Params.Has("quiet"));

	if (Params.Has("help"))
	{
		std::cout << "Usage: mavi [options] [script.as] [arguments]\n\n";
		std::cout << "Options:";
		std::cout << "\n\t--help: show this message";
		std::cout << "\n\t--version: show mavi version details";
		std::cout << "\n\t--plain: disable stdout colors";
		std::cout << "\n\t--debug: start script using debugger";
		std::cout << std::endl;
		Output->Detach();
		return JUMP_CODE + EXIT_SUCCESS;
	}
	else if (Params.Has("version", "v"))
	{
		String Message = Mavi::Library::GetDetails();
		std::cout << Message << std::endl;
		Output->Detach();
		return JUMP_CODE + EXIT_SUCCESS;
	}
	
	String Directory = OS::Directory::Get();
	FileEntry Context; size_t Index = 0;

	for (size_t i = 1; i < Args.size(); i++)
	{
		auto Path = OS::Path::Resolve(Args[i], Directory);
		if (OS::File::State(Path, &Context) || OS::File::State(Path + ".as", &Context))
		{
			Index = i;
			break;
		}
	}

	if (!Context.IsExists)
	{
		VI_ERR("[mavi] provide a path to existing script file");
		Output->Detach();
		return JUMP_CODE + EXIT_INPUT_FAILURE;
	}

	CatchAll();
	if (Index > 0)
		Args.erase(Args.begin(), Args.begin() + Index + 1);

	Multiplexer::Create();
	AttachDebugger = Params.Has("debug");
	int ExitCode = Execute(Context.Path, OS::File::ReadAsString(Context.Path.c_str()), Args);
	VI_RELEASE(Unit);
	VI_RELEASE(VM);

	Output->Detach();
	return ExitCode;
}
int main(int argc, char* argv[])
{
    Mavi::Initialize((size_t)Mavi::Preset::Game);
	int ExitCode = Dispatch(argv, argc);
	Mavi::Uninitialize();

	return ExitCode;
}