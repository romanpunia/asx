#ifndef RUNTIME_H
#define RUNTIME_H
#include <vengeance/bindings.h>
#include <vengeance/vengeance.h>

using namespace Vitex::Core;
using namespace Vitex::Compute;
using namespace Vitex::Scripting;

namespace ASX
{
	enum class ExitStatus
	{
		Continue = 0x00fffff - 1,
		OK = 0,
		RuntimeError,
		PrepareError,
		LoadingError,
		SavingError,
		CompilerError,
		EntrypointError,
		InputError,
		InvalidCommand,
		InvalidDeclaration,
		CommandError,
		Kill
	};

	struct ProgramEntrypoint
	{
		const char* ReturnsWithArgs = "int main(array<string>@)";
		const char* Returns = "int main()";
		const char* Simple = "void main()";
	};

	struct EnvironmentConfig
	{
		InlineArgs Commandline;
		UnorderedSet<String> Addons;
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
		int32_t AutoSchedule;
		bool AutoConsole;
		bool AutoStop;
		bool Inline;

		EnvironmentConfig() : ThisCompiler(nullptr), Module("__anonymous__"), AutoSchedule(-1), AutoConsole(false), AutoStop(false), Inline(true)
		{
		}
		void Parse(int ArgsCount, char** ArgsData, const UnorderedSet<String>& Flags = { })
		{
			Commandline = OS::Process::ParseArgs(ArgsCount, ArgsData, (size_t)ArgsFormat::KeyValue | (size_t)ArgsFormat::FlagValue | (size_t)ArgsFormat::StopIfNoMatch, Flags);
		}
		static EnvironmentConfig& Get(EnvironmentConfig* Other = nullptr)
		{
			static EnvironmentConfig* Base = Other;
			VI_ASSERT(Base != nullptr, "env was not set");
			return *Base;
		}
	};

	struct SystemConfig
	{
		UnorderedMap<String, std::pair<String, String>> Functions;
		UnorderedMap<AccessOption, bool> Permissions;
		Vector<std::pair<String, bool>> Libraries;
		Vector<std::pair<String, int32_t>> Settings;
		Vector<String> SystemAddons;
		bool TsImports = true;
		bool Tags = true;
		bool Debug = false;
		bool Interactive = false;
		bool EssentialsOnly = true;
		bool PrettyProgress = true;
		bool LoadByteCode = false;
		bool SaveByteCode = false;
		bool SaveSourceCode = false;
		bool Dependencies = false;
		bool Install = false;
		size_t Installed = 0;
	};

	class Runtime
	{
	public:
		static void StartupEnvironment(EnvironmentConfig& Env)
		{
			if (Env.AutoSchedule >= 0)
				Schedule::Get()->Start(Env.AutoSchedule > 0 ? Schedule::Desc((size_t)Env.AutoSchedule) : Schedule::Desc());

			if (Env.AutoConsole)
				Console::Get()->Attach();
		}
		static void ShutdownEnvironment(EnvironmentConfig& Env)
		{
			if (Env.AutoStop)
				Schedule::Get()->Stop();
		}
		static void ConfigureSystem(SystemConfig& Config)
		{
			for (auto& Option : Config.Permissions)
				OS::Control::Set(Option.first, Option.second);
		}
		static bool ConfigureContext(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, Compiler* ThisCompiler)
		{
			VM->SetTsImports(Config.TsImports);
			VM->SetModuleDirectory(OS::Path::GetDirectory(Env.Path.c_str()));
			VM->SetPreserveSourceCode(Config.SaveSourceCode);

			for (auto& Name : Config.SystemAddons)
			{
				if (!VM->ImportSystemAddon(Name))
				{
					VI_ERR("system addon <%s> cannot be loaded", Name.c_str());
					return false;
				}
			}

			for (auto& Path : Config.Libraries)
			{
				if (!VM->ImportCLibrary(Path.first, Path.second))
				{
					VI_ERR("external %s <%s> cannot be loaded", Path.second ? "addon" : "clibrary", Path.first.c_str());
					return false;
				}
			}

			for (auto& Data : Config.Functions)
			{
				if (!VM->ImportCFunction({ Data.first }, Data.second.first, Data.second.second))
				{
					VI_ERR("clibrary function <%s> from <%s> cannot be loaded", Data.second.first.c_str(), Data.first.c_str());
					return false;
				}
			}

			auto* Macro = ThisCompiler->GetProcessor();
			Macro->AddDefaultDefinitions();

			Env.ThisCompiler = ThisCompiler;
			Bindings::Tags::BindSyntax(VM, Config.Tags, &Runtime::ProcessTags);
			EnvironmentConfig::Get(&Env);

			VM->ImportSystemAddon("ctypes");
			VM->BeginNamespace("this_process");
			VM->SetFunctionDef("void exit_event(int)");
			VM->SetFunction("void before_exit(exit_event@)", &Runtime::ApplyContextExit);
			VM->SetFunction("uptr@ get_compiler()", &Runtime::GetCompiler);
			VM->EndNamespace();
			return true;
		}
		static bool TryContextExit(EnvironmentConfig& Env, int Value)
		{
			if (!Env.AtExit.IsValid())
				return false;

			auto Status = Env.AtExit([Value](ImmediateContext* Context)
			{
				Context->SetArg32(0, Value);
			}).Get();
			Env.AtExit.Release();
			VirtualMachine::CleanupThisThread();
			return !!Status;
		}
		static void ApplyContextExit(asIScriptFunction* Callback)
		{
			auto& Env = EnvironmentConfig::Get();
			UPtr<ImmediateContext> Context = Callback ? Env.ThisCompiler->GetVM()->RequestContext() : nullptr;
			Env.AtExit = FunctionDelegate(Callback, *Context);
		}
		static void AwaitContext(std::mutex& Mutex, EventLoop* Loop, VirtualMachine* VM, ImmediateContext* Context)
		{
			EventLoop::Set(Loop);
			while (Loop->PollExtended(Context, 1000))
			{
				VM->PerformPeriodicGarbageCollection(60000);
				Loop->Dequeue(VM);
			}

			UMutex<std::mutex> Unique(Mutex);
			if (Schedule::HasInstance())
			{
				auto* Queue = Schedule::Get();
				while (!Queue->CanEnqueue() && Queue->HasAnyTasks())
					Queue->Dispatch();
			}

			EventLoop::Set(nullptr);
			Context->Reset();
			VM->PerformFullGarbageCollection();
			ApplyContextExit(nullptr);
		}
		static Function GetEntrypoint(EnvironmentConfig& Env, ProgramEntrypoint& Entrypoint, Compiler* Unit, bool Silent = false)
		{
			Function MainReturnsWithArgs = Unit->GetModule().GetFunctionByDecl(Entrypoint.ReturnsWithArgs);
			Function MainReturns = Unit->GetModule().GetFunctionByDecl(Entrypoint.Returns);
			Function MainSimple = Unit->GetModule().GetFunctionByDecl(Entrypoint.Simple);
			if (MainReturnsWithArgs.IsValid() || MainReturns.IsValid() || MainSimple.IsValid())
				return MainReturnsWithArgs.IsValid() ? MainReturnsWithArgs : (MainReturns.IsValid() ? MainReturns : MainSimple);

			if (!Silent)
				VI_ERR("module %s must contain either: <%s>, <%s> or <%s>", Env.Module, Entrypoint.ReturnsWithArgs, Entrypoint.Returns, Entrypoint.Simple);
			return Function(nullptr);
		}
		static Compiler* GetCompiler()
		{
			return EnvironmentConfig::Get().ThisCompiler;
		}

	private:
		static void ProcessTags(VirtualMachine* VM, Bindings::Tags::TagInfo&& Info)
		{
			auto& Env = EnvironmentConfig::Get();
			for (auto& Tag : Info)
			{
				if (Tag.Name != "main")
					continue;

				for (auto& Directive : Tag.Directives)
				{
					if (Directive.Name == "#schedule::main")
					{
						auto Threads = Directive.Args.find("threads");
						if (Threads != Directive.Args.end())
							Env.AutoSchedule = FromString<uint8_t>(Threads->second).Or(0);
						else
							Env.AutoSchedule = 0;

						auto Stop = Directive.Args.find("stop");
						if (Stop != Directive.Args.end())
						{
							Stringify::ToLower(Threads->second);
							auto Value = FromString<uint8_t>(Threads->second);
							if (!Value)
								Env.AutoStop = (Threads->second == "on" || Threads->second == "true" || Threads->second == "yes");
							else
								Env.AutoStop = *Value > 0;
						}
					}
					else if (Directive.Name == "#console::main")
						Env.AutoConsole = true;
				}
			}
		}
	};
}
#endif