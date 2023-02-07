#include <edge/edge.h>
#include <edge/core/script.h>
#include <edge/core/bindings.h>
#include <signal.h>

using namespace Edge::Core;
using namespace Edge::Compute;
using namespace Edge::Script;

static std::vector<std::string> Args;
static VMManager* VM = nullptr;
static VMCompiler* Compiler = nullptr;
static const char* ModuleName = "entry-point";
static const char* Entrypoint1 = "int main(array<string>@)";
static const char* Entrypoint2 = "int main()";

int Abort(const char* Signal, bool Normal)
{
	if (Normal)
	{
		std::exit(0);
		return 0;
	}

	std::string StackTrace;
	VMContext* Context = VMContext::Get();
	if (Context != nullptr)
		StackTrace = Context->Get()->GetStackTrace(0, 64);
	else
		StackTrace = OS::GetStackTrace(0, 32);

	OS::SetLogActive(true);
	ED_ERR("[calculus] runtime error detected (%s); %s", Signal, StackTrace.c_str());

	std::exit(-1);
	return -1;
}
int Execute(const std::string& Path, const std::string& Data)
{
	auto* Queue = Schedule::Get();
	Queue->SetImmediate(true);

	VM = new VMManager();
	VM->SetImports((uint32_t)VMImport::All);
	VM->SetDocumentRoot(OS::Path::GetDirectory(Path.c_str()));
	VM->SetCompilerErrorCallback([]()
	{
		OS::SetLogActive(true);
	});

	Compiler = VM->CreateCompiler();
	if (Compiler->Prepare(ModuleName, true) < 0)
	{
		OS::SetLogActive(true);
		ED_ERR("[calculus] cannot prepare <%s> module scope", ModuleName);
		return 1;
	}

	if (Compiler->LoadCode(Path, Data.c_str(), Data.size()) < 0)
	{
		OS::SetLogActive(true);
		ED_ERR("[calculus] cannot load <%s> module script code", ModuleName);
		return 2;
	}

	if (Compiler->Compile().Get() < 0)
	{
		OS::SetLogActive(true);
		ED_ERR("[calculus] cannot compile <%s> module", ModuleName);
		return 3;
	}

	VMFunction Main1 = Compiler->GetModule().GetFunctionByDecl(Entrypoint1);
	VMFunction Main2 = Compiler->GetModule().GetFunctionByDecl(Entrypoint2);
	if (!Main1.IsValid() && !Main2.IsValid())
	{
		OS::SetLogActive(true);
		ED_ERR("[calculus] module %s must contain either: <%s> or <%s>", ModuleName, Entrypoint1, Entrypoint2);
		return 4;
	}

	VMTypeInfo Type = VM->Global().GetTypeInfoByDecl("array<string>@");
	VMContext* Context = Compiler->GetContext();
	Bindings::Array* Params = Bindings::Array::Compose<std::string>(Type.GetTypeInfo(), Args);
	Context->TryExecute(Main1.IsValid() ? Main1 : Main2, [&Main1, Params](VMContext* Context)
	{
		if (Main1.IsValid())
			Context->SetArgObject(0, Params);
	}).Wait();

	VM->ReleaseObject(Params, Type);
	return (int)Context->GetReturnDWord();
}
int Dispatch(char** ArgsData, int ArgsCount)
{
#ifdef TH_UNIX
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#endif
	signal(SIGABRT, [](int) { Abort("abort", false); });
	signal(SIGFPE, [](int) { Abort("division by zero", false); });
	signal(SIGILL, [](int) { Abort("illegal instruction", false); });
	signal(SIGINT, [](int) { Abort("interrupt", true); });
	signal(SIGSEGV, [](int) { Abort("segmentation fault", false); });
	signal(SIGTERM, [](int) { Abort("termination", true); });

	auto* Output = Console::Get();
	Output->Show();

	Args.reserve((size_t)ArgsCount);
	for (int i = 0; i < ArgsCount; i++)
		Args.push_back(ArgsData[i]);

	OS::Process::ArgsContext Params(ArgsCount, ArgsData);
	OS::SetLogPretty(!Params.Has("no-colors"));
	OS::SetLogActive(!Params.Has("no-logging"));

	if (Params.Has("help"))
	{
		std::cout << "Usage: edge [options] [script.as] [arguments]\n\n";
		std::cout << "Options:";
		std::cout << "\n\t--help: show this message";
		std::cout << "\n\t-v, --version: show edge version details";
		std::cout << "\n\t--no-colors: disable stdout colors";
		std::cout << "\n\t--verbose: enable logging messages";
		std::cout << std::endl;
		return 0;
	}
	else if (Params.Has("version", "v"))
	{
		std::string Message = Edge::Library::Details();
		std::cout << Message << std::endl;
		return 0;
	}
	
	std::string Directory = OS::Directory::Get();
	FileEntry Context; size_t Index = 0;

	for (size_t i = 1; i < Args.size(); i++)
	{
		auto& Path = OS::Path::Resolve(Args[i], Directory);
		if (OS::File::State(Path, &Context))
		{
			Index = i;
			break;
		}
	}

	if (!Context.IsExists)
	{
		OS::SetLogActive(true);
		ED_ERR("[calculus] provide a path to existing script file");
		return 0;
	}

	if (Index > 0)
		Args.erase(Args.begin(), Args.begin() + Index + 1);

	int ExitCode = Execute(Context.Path, OS::File::ReadAsString(Context.Path.c_str()));
	ED_RELEASE(Compiler);
	ED_RELEASE(VM);

	return ExitCode;
}
int main(int argc, char* argv[])
{
    Edge::Initialize((size_t)(Edge::Init::Core | Edge::Init::Network | Edge::Init::SSL | Edge::Init::SDL2 | Edge::Init::Compute | Edge::Init::Locale | Edge::Init::Audio | Edge::Init::GLEW));
    int ExitCode = Dispatch(argv, argc);
    Edge::Uninitialize();

    return ExitCode;
}