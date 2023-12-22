#ifndef APP_H
#define APP_H
#include <mavi/mavi.h>
#include <mavi/core/scripting.h>
#include <mavi/core/bindings.h>
#include <mavi/core/network.h>
#define JUMP_CODE 0x00fffff
#define EXIT_CONTINUE -1
#define EXIT_OK 0x0
#define EXIT_RUNTIME_FAILURE 0x1
#define EXIT_PREPARE_FAILURE 0x2
#define EXIT_LOADING_FAILURE 0x3
#define EXIT_SAVING_FAILURE 0x4
#define EXIT_COMPILER_FAILURE 0x5
#define EXIT_ENTRYPOINT_FAILURE 0x6
#define EXIT_INPUT_FAILURE 0x7
#define EXIT_INVALID_COMMAND 0x8
#define EXIT_INVALID_DECLARATION 0x9
#define EXIT_COMMAND_FAILURE 0x10
#define EXIT_KILL 0x11
#define COMMAND_GIT_EXIT_OK 0x1
#define COMMAND_CMAKE_EXIT_OK 0x0
#define REPOSITORY_TEMPLATE_ADDON "https://github.com/romanpunia/addon.as"
#define REPOSITORY_TEMPLATE_EXECUTABLE "https://github.com/romanpunia/executable.as"
#define REPOSITORY_TARGET_MAVI "https://github.com/romanpunia/mavi"
#define REPOSITORY_SOURCE "https://github.com/"
#define FILE_INDEX "addon.as"
#define FILE_ADDON "addon.json"

using namespace Mavi::Core;
using namespace Mavi::Compute;
using namespace Mavi::Engine;
using namespace Mavi::Network;
using namespace Mavi::Scripting;

typedef std::function<int(const String&)> CommandCallback;

struct ProgramCommand
{
	CommandCallback Callback;
	String Description;
};

struct ProgramContext
{
	OS::Process::ArgsContext Params;
	UnorderedSet<String> Addons;
	Vector<String> Args;
	FunctionDelegate AtExit;
	FileEntry File;
	String Name;
	String Path;
	String Program;
	String Registry;
	String Mode;
	String Output;
	String Addon;
	Compiler* ThisCompiler;
	const char* Module;
	bool Inline;

	ProgramContext(int ArgsCount, char** ArgsData) : Params(ArgsCount, ArgsData), ThisCompiler(nullptr), Module("__anonymous__"), Inline(true)
	{
		Args.reserve((size_t)ArgsCount);
		for (int i = 0; i < ArgsCount; i++)
			Args.push_back(ArgsData[i]);
	}
	static ProgramContext& Get(ProgramContext* Other = nullptr)
	{
		static ProgramContext* Base = Other;
		VI_ASSERT(Base != nullptr, "context was not set");
		return *Base;
	}
};

struct ProgramEntrypoint
{
	const char* ReturnsWithArgs = "int main(array<string>@)";
	const char* Returns = "int main()";
	const char* Simple = "void main()";
};

struct ProgramConfig
{
	UnorderedMap<String, std::pair<String, String>> Functions;
	Vector<std::pair<String, bool>> Libraries;
	Vector<std::pair<String, int32_t>> Settings;
	Vector<String> SystemAddons;
	bool TsImports = true;
	bool Addons = true;
	bool CLibraries = true;
	bool CFunctions = true;
	bool Files = true;
	bool Remotes = true;
	bool Debug = false;
	bool Translator = false;
	bool Interactive = false;
	bool EssentialsOnly = true;
	bool LoadByteCode = false;
	bool SaveByteCode = false;
	bool SaveSourceCode = false;
	bool Dependencies = false;
	bool Install = false;
	size_t Installed = 0;
};

Compiler* GetThisCompiler()
{
	return ProgramContext::Get().ThisCompiler;
}
void AtExitContext(asIScriptFunction* Callback)
{
	auto& Contextual = ProgramContext::Get();
	ImmediateContext* Context = Callback ? Contextual.ThisCompiler->GetVM()->RequestContext() : nullptr;
	Contextual.AtExit = FunctionDelegate(Callback, Context);
	VI_RELEASE(Context);
}
void AwaitContext(Schedule* Queue, EventLoop* Loop, VirtualMachine* VM, ImmediateContext* Context)
{
	EventLoop::Set(Loop);
	while (Loop->PollExtended(Context, 1000))
		Loop->Dequeue(VM);

	while (!Queue->CanEnqueue() && Queue->HasAnyTasks())
		Queue->Dispatch();

	Queue->Stop();
	EventLoop::Set(nullptr);
	Context->Reset();
	VM->PerformFullGarbageCollection();
	AtExitContext(nullptr);
}
int ConfigureEngine(ProgramConfig& Config, ProgramContext& Contextual, VirtualMachine* VM, Compiler* ThisCompiler)
{
	uint32_t ImportOptions = 0;
	if (Config.Addons)
		ImportOptions |= (uint32_t)Imports::Addons;
	if (Config.CLibraries)
		ImportOptions |= (uint32_t)Imports::CLibraries;
	if (Config.CFunctions)
		ImportOptions |= (uint32_t)Imports::CFunctions;
	if (Config.Files)
		ImportOptions |= (uint32_t)Imports::Files;
	if (Config.Remotes)
		ImportOptions |= (uint32_t)Imports::Remotes;

	VM->SetTsImports(Config.TsImports);
	VM->SetModuleDirectory(OS::Path::GetDirectory(Contextual.Path.c_str()));
	VM->SetPreserveSourceCode(Config.SaveSourceCode);
	VM->SetImports(ImportOptions);

	if (Config.Translator)
		VM->SetByteCodeTranslator((uint32_t)TranslationOptions::Optimal);

	for (auto& Name : Config.SystemAddons)
	{
		if (!VM->ImportSystemAddon(Name))
		{
			VI_ERR("system addon <%s> cannot be loaded", Name.c_str());
			return JUMP_CODE + EXIT_LOADING_FAILURE;
		}
	}

	for (auto& Path : Config.Libraries)
	{
		if (!VM->ImportCLibrary(Path.first, Path.second))
		{
			VI_ERR("external %s <%s> cannot be loaded", Path.second ? "addon" : "clibrary", Path.first.c_str());
			return JUMP_CODE + EXIT_LOADING_FAILURE;
		}
	}

	for (auto& Data : Config.Functions)
	{
		if (!VM->ImportCFunction({ Data.first }, Data.second.first, Data.second.second))
		{
			VI_ERR("clibrary function <%s> from <%s> cannot be loaded", Data.second.first.c_str(), Data.first.c_str());
			return JUMP_CODE + EXIT_LOADING_FAILURE;
		}
	}
	
	auto* Macro = ThisCompiler->GetProcessor();
	Macro->AddDefaultDefinitions();

	Contextual.ThisCompiler = ThisCompiler;
	ProgramContext::Get(&Contextual);

	VM->ImportSystemAddon("ctypes");
	VM->BeginNamespace("this_process");
	VM->SetFunctionDef("void exit_event(int)");
	VM->SetFunction("void before_exit(exit_event@)", &AtExitContext);
	VM->SetFunction("uptr@ get_compiler()", &GetThisCompiler);
	VM->EndNamespace();
	return 0;
}
bool TryContextExit(ProgramContext& Contextual, int Value)
{
	if (!Contextual.AtExit.IsValid())
		return false;
	
	auto Status = Contextual.AtExit([Value](ImmediateContext* Context)
	{
		Context->SetArg32(0, Value);
	}).Get();
	Contextual.AtExit.Release();
	VirtualMachine::CleanupThisThread();
	return !!Status;
}
Function GetEntrypoint(ProgramContext& Contextual, ProgramEntrypoint& Entrypoint, Compiler* Unit)
{
	Function MainReturnsWithArgs = Unit->GetModule().GetFunctionByDecl(Entrypoint.ReturnsWithArgs);
	Function MainReturns = Unit->GetModule().GetFunctionByDecl(Entrypoint.Returns);
	Function MainSimple = Unit->GetModule().GetFunctionByDecl(Entrypoint.Simple);
	if (MainReturnsWithArgs.IsValid() || MainReturns.IsValid() || MainSimple.IsValid())
		return MainReturnsWithArgs.IsValid() ? MainReturnsWithArgs : (MainReturns.IsValid() ? MainReturns : MainSimple);

	VI_ERR("module %s must contain either: <%s>, <%s> or <%s>", Contextual.Module, Entrypoint.ReturnsWithArgs, Entrypoint.Returns, Entrypoint.Simple);
	return Function(nullptr);
}
#endif