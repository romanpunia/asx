#include <mavi/mavi.h>
#include <mavi/core/scripting.h>
#include <mavi/core/bindings.h>
#include <mavi/core/network.h>
#include <signal.h>

using namespace Mavi::Core;
using namespace Mavi::Compute;
using namespace Mavi::Engine;
using namespace Mavi::Scripting;

static VirtualMachine* VM = nullptr;
static Mavi::Scripting::Compiler* Unit = nullptr;
static const char* ModuleName = "entry-point";
static const char* Entrypoint1 = "int main(array<string>@)";
static const char* Entrypoint2 = "int main()";
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
	if (Normal)
	{
		static size_t Exits = 0;
		auto* App = Application::Get();
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

		return 0;
	}

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

	std::exit(-1);
	return -1;
}
int Execute(const String& Path, const String& Data, const Vector<String>& Args)
{
	auto* Queue = Schedule::Get();
	Queue->SetImmediate(true);

	VM = new VirtualMachine();
	VM->SetImports((uint32_t)Imports::All);
	VM->SetDocumentRoot(OS::Path::GetDirectory(Path.c_str()));

	Unit = VM->CreateCompiler();
	if (Unit->Prepare(ModuleName, true) < 0)
	{
		VI_ERR("[mavi] cannot prepare <%s> module scope", ModuleName);
		return 1;
	}

	if (Unit->LoadCode(Path, Data.c_str(), Data.size()) < 0)
	{
		VI_ERR("[mavi] cannot load <%s> module script code", ModuleName);
		return 2;
	}

	if (Unit->Compile().Get() < 0)
	{
		VI_ERR("[mavi] cannot compile <%s> module", ModuleName);
		return 3;
	}

	Function Main1 = Unit->GetModule().GetFunctionByDecl(Entrypoint1);
	Function Main2 = Unit->GetModule().GetFunctionByDecl(Entrypoint2);
	if (!Main1.IsValid() && !Main2.IsValid())
	{
		VI_ERR("[mavi] module %s must contain either: <%s> or <%s>", ModuleName, Entrypoint1, Entrypoint2);
		return 4;
	}

	ImmediateContext* Context = Unit->GetContext();
	Context->SetExceptionCallback([](ImmediateContext* Context)
	{
		if (!Context->WillExceptionBeCaught())
			Abort(Context->GetExceptionString(), false, false);
	});

	TypeInfo Type = VM->GetTypeInfoByDecl("array<string>@");
	Bindings::Array* Params = Bindings::Array::Compose<String>(Type.GetTypeInfo(), Args);
	Context->TryExecute(false, Main1.IsValid() ? Main1 : Main2, [&Main1, Params](ImmediateContext* Context)
	{
		if (Main1.IsValid())
			Context->SetArgObject(0, Params);
	}).Wait();

	int ExitCode = (int)Context->GetReturnDWord();
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
	OS::SetLogPretty(!Params.Has("no-colors"));
	OS::SetLogActive(!Params.Has("no-logging"));

	if (Params.Has("help"))
	{
		std::cout << "Usage: mavi [options] [script.as] [arguments]\n\n";
		std::cout << "Options:";
		std::cout << "\n\t--help: show this message";
		std::cout << "\n\t-v, --version: show mavi version details";
		std::cout << "\n\t--no-colors: disable stdout colors";
		std::cout << "\n\t--verbose: enable logging messages";
		std::cout << std::endl;
		return 0;
	}
	else if (Params.Has("version", "v"))
	{
		String Message = Mavi::Library::GetDetails();
		std::cout << Message << std::endl;
		return 0;
	}
	
	String Directory = OS::Directory::Get();
	FileEntry Context; size_t Index = 0;

	for (size_t i = 1; i < Args.size(); i++)
	{
		auto Path = OS::Path::Resolve(Args[i], Directory);
		if (OS::File::State(Path, &Context))
		{
			Index = i;
			break;
		}
	}

	if (!Context.IsExists)
	{
		VI_ERR("[mavi] provide a path to existing script file");
		return 0;
	}

	CatchAll();
	if (Index > 0)
		Args.erase(Args.begin(), Args.begin() + Index + 1);

	int ExitCode = Execute(Context.Path, OS::File::ReadAsString(Context.Path.c_str()), Args);
	VI_RELEASE(Unit);
	VI_RELEASE(VM);

	return ExitCode;
}
int main(int argc, char* argv[])
{
    Mavi::Initialize((size_t)Mavi::Preset::Game);
	int ExitCode = Dispatch(argv, argc);
	Mavi::Uninitialize();

	return ExitCode;
}