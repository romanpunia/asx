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
	Vector<String> Args;
	FileEntry File;
	String Path;
	String Program;
	const char* Module;
	bool Inline;

	ProgramContext(int ArgsCount, char** ArgsData) : Params(ArgsCount, ArgsData), Module("__anonymous__"), Inline(true)
	{
		Args.reserve((size_t)ArgsCount);
		for (int i = 0; i < ArgsCount; i++)
			Args.push_back(ArgsData[i]);
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
	UnorderedMap<String, std::pair<String, String>> Symbols;
	Vector<String> Submodules;
	Vector<String> Addons;
	Vector<String> Libraries;
	Vector<std::pair<String, int32_t>> Settings;
	bool Modules = true;
	bool CLibraries = true;
	bool CSymbols = true;
	bool Files = true;
	bool JSON = true;
	bool Remotes = true;
	bool Debug = false;
	bool Translator = false;
	bool Interactive = false;
	bool EssentialsOnly = true;
	bool LoadByteCode = false;
	bool SaveByteCode = false;
	bool SaveSourceCode = false;
};

void AwaitContext(Schedule* Queue, VirtualMachine* VM, ImmediateContext* Context)
{
	while (Queue->IsActive() || Context->GetState() == Activation::Active || Context->IsPending())
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	while (!Queue->CanEnqueue() && Queue->HasAnyTasks())
		Queue->Dispatch();

	Context->Unprepare();
	VM->Collect();
}
int ConfigureEngine(ProgramConfig& Config, ProgramContext& Contextual, VirtualMachine* VM)
{
	uint32_t ImportOptions = 0;
	if (Config.Modules)
		ImportOptions |= (uint32_t)Imports::Submodules;
	if (Config.CLibraries)
		ImportOptions |= (uint32_t)Imports::CLibraries;
	if (Config.CSymbols)
		ImportOptions |= (uint32_t)Imports::CSymbols;
	if (Config.Files)
		ImportOptions |= (uint32_t)Imports::Files;
	if (Config.JSON)
		ImportOptions |= (uint32_t)Imports::JSON;
	if (Config.Remotes)
		ImportOptions |= (uint32_t)Imports::Remotes;

	VM->SetModuleDirectory(OS::Path::GetDirectory(Contextual.Path.c_str()));
	VM->SetPreserveSourceCode(Config.SaveSourceCode);
	VM->SetImports(ImportOptions);

	if (Config.Translator)
		VM->SetByteCodeTranslator((uint32_t)TranslationOptions::Optimal);

	for (auto& Name : Config.Submodules)
	{
		if (!VM->ImportSubmodule(Name))
		{
			VI_ERR("system module <%s> cannot be loaded", Name.c_str());
			return JUMP_CODE + EXIT_LOADING_FAILURE;
		}
	}

	for (auto& Path : Config.Addons)
	{
		if (!VM->ImportLibrary(Path, true))
		{
			VI_ERR("external module <%s> cannot be loaded", Path.c_str());
			return JUMP_CODE + EXIT_LOADING_FAILURE;
		}
	}

	for (auto& Path : Config.Libraries)
	{
		if (!VM->ImportLibrary(Path, false))
		{
			VI_ERR("C library <%s> cannot be loaded", Path.c_str());
			return JUMP_CODE + EXIT_LOADING_FAILURE;
		}
	}

	for (auto& Data : Config.Symbols)
	{
		if (!VM->ImportSymbol({ Data.first }, Data.second.first, Data.second.second))
		{
			VI_ERR("C library symbol <%s> from <%s> cannot be loaded", Data.second.first.c_str(), Data.first.c_str());
			return JUMP_CODE + EXIT_LOADING_FAILURE;
		}
	}

	return 0;
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