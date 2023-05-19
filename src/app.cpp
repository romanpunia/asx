#include <mavi/mavi.h>
#include <mavi/core/scripting.h>
#include <mavi/core/bindings.h>
#include <mavi/core/network.h>
#include <signal.h>
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

class Mavias
{
private:
	typedef std::function<int(const String&)> CommandCallback;

	struct ProgramCommand
	{
		CommandCallback Callback;
		String Description;
	};

public:
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
	} Contextual;

	struct ProgramEntrypoint
	{
		const char* ReturnsWithArgs = "int main(array<string>@)";
		const char* Returns = "int main()";
		const char* Simple = "void main()";
	} Entrypoint;

	struct ProgramConfig
	{
		UnorderedMap<String, std::pair<String, String>> Symbols;
		Vector<String> Submodules;
		Vector<String> Addons;
		Vector<String> Libraries;
		bool Modules = true;
		bool CLibraries = true;
		bool CSymbols = true;
		bool Files = true;
		bool JSON = true;
		bool Debug = false;
		bool Translator = false;
		bool Interactive = false;
		bool EssentialsOnly = false;
		bool LoadByteCode = false;
		bool SaveByteCode = false;
	} Config;

private:
	OrderedMap<String, String, std::greater<String>> Descriptions;
	UnorderedMap<String, ProgramCommand> Commands;
	VirtualMachine* VM;
	Compiler* Unit;

public:
	Mavias(int ArgsCount, char** Args) : Contextual(ArgsCount, Args), VM(nullptr), Unit(nullptr)
	{
		AddDefaultCommands();
		ListenForSignals();
		Config.EssentialsOnly = Contextual.Params.Has("app", "a");
	}
	~Mavias()
	{
		Console::Get()->Detach();
		VI_RELEASE(Unit);
		VI_RELEASE(VM);
	}
	int Dispatch()
	{
		auto* Terminal = Console::Get();
		Terminal->Attach();

		auto* Queue = Schedule::Get();
		Queue->SetImmediate(true);
		
		for (auto& Next : Contextual.Params.Base)
		{
			if (Next.first == "__path__")
				continue;

			auto It = Commands.find(Next.first);
			if (It == Commands.end())
			{
				VI_ERR("command <%s> is not a valid operation", Next.first.c_str());
				return JUMP_CODE + EXIT_INVALID_COMMAND;
			}

			if (!It->second.Callback)
				continue;

			int ExitCode = It->second.Callback(Next.second);
			if (ExitCode != JUMP_CODE + EXIT_CONTINUE)
				return ExitCode;
		}

		if (!Config.Interactive && Contextual.Program.empty())
		{
			Config.Interactive = true;
			if (Contextual.Params.Base.size() > 1)
			{
				VI_ERR("provide a path to existing script file");
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}
		}

		if (Config.Interactive)
		{
			if (Contextual.Path.empty())
				Contextual.Path = OS::Directory::Get();

			if (Config.Debug)
			{
				VI_ERR("invalid option for interactive mode: --debug");
				return JUMP_CODE + EXIT_INVALID_COMMAND;
			}

			if (Config.SaveByteCode)
			{
				VI_ERR("invalid option for interactive mode: --compile");
				return JUMP_CODE + EXIT_INVALID_COMMAND;
			}

			if (Config.LoadByteCode)
			{
				VI_ERR("invalid option for interactive mode: --load");
				return JUMP_CODE + EXIT_INVALID_COMMAND;
			}
		}

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

		VM = new VirtualMachine();
		VM->SetModuleDirectory(OS::Path::GetDirectory(Contextual.Path.c_str()));
		VM->SetImports(ImportOptions);

		if (Config.Translator)
			VM->SetByteCodeTranslator((uint32_t)TranslationOptions::Optimal);

		if (Config.Debug)
			VM->SetDebugger(new DebuggerContext());

		Multiplexer::Create();
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

		Unit = VM->CreateCompiler();
		if (Unit->Prepare(Contextual.Module) < 0)
		{
			VI_ERR("cannot prepare <%s> module scope", Contextual.Module);
			return JUMP_CODE + EXIT_PREPARE_FAILURE;
		}

		if (Config.Interactive)
		{
			String Data, Multidata;
			Data.reserve(1024 * 1024);
			VM->ImportSubmodule("std");
			PrintIntroduction();

			auto* Debugger = new DebuggerContext(DebugType::Detach);
			char DefaultCode[] = "void main(){}";
			bool Editor = false;
			size_t Section = 0;

			Contextual.Path += Contextual.Module;
			Debugger->SetEngine(VM);

			if (Unit->LoadCode(Contextual.Path + ":0", DefaultCode, sizeof(DefaultCode) - 1) < 0)
			{
				VI_ERR("cannot load default entrypoint for interactive mode");
				VI_RELEASE(Debugger);
				return JUMP_CODE + EXIT_LOADING_FAILURE;
			}

			if (Unit->Compile().Get() < 0)
			{
				VI_ERR("cannot compile default module for interactive mode");
				VI_RELEASE(Debugger);
				return JUMP_CODE + EXIT_COMPILER_FAILURE;
			}

			for (;;)
			{
				if (!Editor)
					std::cout << "> ";
	
				if (!Terminal->ReadLine(Data, Data.capacity()))
				{
					if (!Editor)
						break;

				ExitEditor:
					Editor = false;
					Data = Multidata;
					Multidata.clear();
					goto Execute;
				}
				else if (Data.empty())
					continue;

				Stringify(&Data).Trim();
				if (Editor)
				{
					if (!Data.empty() && Data.back() == '\x4')
					{
						Data.erase(Data.end() - 1);
						if (Contextual.Inline && !Data.empty() && Data.back() != ';')
							Data += ';';

						Multidata += Data;
						goto ExitEditor;
					}

					if (Contextual.Inline && !Data.empty() && Data.back() != ';')
						Data += ';';

					Multidata += Data;
					continue;
				}
				else if (Data == ".help")
				{
					std::cout << "  .mode   - switch between registering and executing the code" << std::endl;
					std::cout << "  .help   - show available commands" << std::endl;
					std::cout << "  .editor - enter editor mode" << std::endl;
					std::cout << "  .exit   - exit interactive mode" << std::endl;
					std::cout << "  *       - anything else will be interpreted as script code" << std::endl;
					continue;
				}
				else if (Data == ".mode")
				{
					Contextual.Inline = !Contextual.Inline;
					if (Contextual.Inline)
						std::cout << "  evaluation mode: you may now execute your code" << std::endl;
					else
						std::cout << "  register mode: you may now register script interfaces" << std::endl;
					continue;
				}
				else if (Data == ".editor")
				{
					std::cout << "  editor mode: you may write multiple lines of code (Ctrl+D to finish)\n" << std::endl;
					Editor = true;
					continue;
				}
				else if (Data == ".exit")
					break;

			Execute:
				if (Data.empty())
					continue;

				if (Contextual.Inline)
				{
					ImmediateContext* Context = Unit->GetContext();
					if (Unit->ExecuteScoped(Data, "any@").Get() == (int)Activation::FINISHED)
					{
						String Indent = "  ";
						auto* Value = Context->GetReturnObject<Bindings::Any>();
						std::cout << Indent << Debugger->ToString(Indent, 3, Value, VM->GetTypeInfoByName("any").GetTypeId()) << std::endl;
					}
					else
						Context->Abort();
					Context->Unprepare();
				}
				else
				{
					String Index = ":" + ToString(++Section);
					if (Unit->LoadCode(Contextual.Path + Index, Data.c_str(), Data.size()) < 0 || Unit->Compile().Get() < 0)
						continue;
				}
			}

			VI_RELEASE(Debugger);
			return JUMP_CODE + EXIT_OK;
		}

		if (!Config.LoadByteCode)
		{
			if (Unit->LoadCode(Contextual.Path, Contextual.Program.c_str(), Contextual.Program.size()) < 0)
			{
				VI_ERR("cannot load <%s> module script code", Contextual.Module);
				return JUMP_CODE + EXIT_LOADING_FAILURE;
			}

			if (Unit->Compile().Get() < 0)
			{
				VI_ERR("cannot compile <%s> module", Contextual.Module);
				return JUMP_CODE + EXIT_COMPILER_FAILURE;
			}
		}
		else
		{
			ByteCodeInfo Info;
			Info.Data.insert(Info.Data.begin(), Contextual.Program.begin(), Contextual.Program.end());
			if (Unit->LoadByteCode(&Info).Get() < 0)
			{
				VI_ERR("cannot load <%s> module bytecode", Contextual.Module);
				return JUMP_CODE + EXIT_LOADING_FAILURE;
			}
		}

		if (Config.SaveByteCode)
		{
			ByteCodeInfo Info;
			Info.Debug = Config.Debug;
			if (Unit->SaveByteCode(&Info) >= 0 && OS::File::Write(Contextual.Path + ".gz", (const char*)Info.Data.data(), Info.Data.size()))
				return JUMP_CODE + EXIT_OK;

			VI_ERR("cannot save <%s> module bytecode", Contextual.Module);
			return JUMP_CODE + EXIT_SAVING_FAILURE;
		}

		Function MainReturnsWithArgs = Unit->GetModule().GetFunctionByDecl(Entrypoint.ReturnsWithArgs);
		Function MainReturns = Unit->GetModule().GetFunctionByDecl(Entrypoint.Returns);
		Function MainSimple = Unit->GetModule().GetFunctionByDecl(Entrypoint.Simple);
		if (!MainReturnsWithArgs.IsValid() && !MainReturns.IsValid() && !MainSimple.IsValid())
		{
			VI_ERR("module %s must contain either: <%s>, <%s> or <%s>", Contextual.Module, Entrypoint.ReturnsWithArgs, Entrypoint.Returns, Entrypoint.Simple);
			return JUMP_CODE + EXIT_ENTRYPOINT_FAILURE;
		}

		if (Config.Debug)
			PrintIntroduction();

		ImmediateContext* Context = Unit->GetContext();
		Context->SetExceptionCallback([this](ImmediateContext* Context)
		{
			if (!Context->WillExceptionBeCaught())
			{
				VI_ERR("program has failed to catch an exception; killed");
				std::exit(JUMP_CODE + EXIT_RUNTIME_FAILURE);
			}
		});

		TypeInfo Type = VM->GetTypeInfoByDecl("array<string>@");
		Bindings::Array* ArgsArray = Bindings::Array::Compose<String>(Type.GetTypeInfo(), Contextual.Args);
		Function Entrypoint = MainReturnsWithArgs.IsValid() ? MainReturnsWithArgs : (MainReturns.IsValid() ? MainReturns : MainSimple);
		Context->Execute(Entrypoint, [&Entrypoint, ArgsArray](ImmediateContext* Context)
		{
			if (Entrypoint.GetArgsCount() > 0)
				Context->SetArgObject(0, ArgsArray);
		}).Wait();

		int ExitCode = !MainReturnsWithArgs.IsValid() && !MainReturns.IsValid() && MainSimple.IsValid() ? 0 : (int)Context->GetReturnDWord();
		VM->ReleaseObject(ArgsArray, Type);

		while (Queue->IsActive() || Context->GetState() == Activation::ACTIVE || Context->IsPending())
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

		while (!Queue->CanEnqueue() && Queue->HasAnyTasks())
			Queue->Dispatch();

		Context->Unprepare();
		VM->Collect();
		return ExitCode;
	}
	void Shutdown()
	{
		static size_t Exits = 0;
		{
			auto* App = Application::Get();
			if (App != nullptr && App->GetState() == ApplicationState::Active)
				goto GracefulShutdown;

			auto* Queue = Schedule::Get();
			if (Queue->IsActive())
				goto GracefulShutdown;

			if (Exits > 0)
				VI_DEBUG("program is not responding; killed");

			return std::exit(0);
		}
	GracefulShutdown:
		++Exits;
		ListenForSignals();
		Application::Get()->Stop();
	}
	void Interrupt()
	{
		if (Config.Debug && VM->GetDebugger()->Interrupt())
			ListenForSignals();
		else
			Shutdown();
	}
	void Abort(const char* Signal)
	{
		String StackTrace;
		ImmediateContext* Context = ImmediateContext::Get();
		if (Context != nullptr)
			StackTrace = Context->Get()->GetStackTrace(0, 64);
		else
			StackTrace = OS::GetStackTrace(0, 32);

		VI_ERR("runtime error detected: %s\n%s", Signal, StackTrace.c_str());
		std::exit(JUMP_CODE + EXIT_RUNTIME_FAILURE);
}

private:
	void AddDefaultCommands()
	{
		AddCommand("-h, --help", "show help message", [this](const String&)
		{
			PrintHelp();
			return JUMP_CODE + EXIT_OK;
		});
		AddCommand("-v, --version", "show version message", [this](const String&)
		{
			PrintIntroduction();
			return JUMP_CODE + EXIT_OK;
		});
		AddCommand("-f, --file", "set target file [expects: path, arguments?]", [this](const String& Path)
		{
			String Directory = OS::Directory::Get();
			size_t Index = 0;

			for (size_t i = 1; i < Contextual.Args.size(); i++)
			{
				auto File = OS::Path::Resolve(Contextual.Args[i], Directory);
				if (OS::File::State(File, &Contextual.File))
				{
					Contextual.Path = File;
					Index = i;
					break;
				}

				File = OS::Path::Resolve(Contextual.Args[i] + (Config.LoadByteCode ? ".as.gz" : ".as"), Directory);
				if (OS::File::State(File, &Contextual.File))
				{
					Contextual.Path = File;
					Index = i;
					break;
				}
			}

			if (!Contextual.File.IsExists)
			{
				VI_ERR("provide a path to existing script file");
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}

			Contextual.Args.erase(Contextual.Args.begin(), Contextual.Args.begin() + Index);
			Contextual.Module = OS::Path::GetFilename(Contextual.Path.c_str());
			Contextual.Program = OS::File::ReadAsString(Contextual.Path.c_str());
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-p, --plain", "disable log colors", [](const String&)
		{
			OS::SetLogPretty(false);
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-q, --quiet", "disable logging", [](const String&)
		{
			OS::SetLogActive(false);
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-d, --debug", "enable debugger interface", [this](const String&)
		{
			Config.Debug = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-j, --jit", "enable jit bytecode translation", [this](const String&)
		{
			Config.Translator = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-i, --interactive", "enable interactive mode", [this](const String& )
		{
			Config.Interactive = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-s, --system", "import system module(s) by name [expects: plus(+) separated list]", [this](const String& Value)
		{
			for (auto& Item : Stringify(&Value).Split('+'))
				Config.Submodules.push_back(Item);

			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-m, --module", "import external module(s) by path [expects: plus(+) separated list]", [this](const String& Value)
		{
			for (auto& Item : Stringify(&Value).Split('+'))
				Config.Addons.push_back(Item);

			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-cl, --clib", "import C library(ies) by path [expects: plus(+) separated list]", [this](const String& Value)
		{
			for (auto& Item : Stringify(&Value).Split('+'))
				Config.Libraries.push_back(Item);

			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-cs, --csymbol", "import C library symbol by declaration [expects: clib_name:cfunc_name=asfunc_decl]", [this](const String& Value)
		{
			size_t Offset1 = Value.find(':');
			if (Offset1 == std::string::npos)
			{
				VI_ERR("invalid C library symbol declaration");
				return JUMP_CODE + EXIT_INVALID_DECLARATION;
			}

			size_t Offset2 = Value.find('=', Offset1);
			if (Offset2 == std::string::npos)
			{
				VI_ERR("invalid C library symbol declaration");
				return JUMP_CODE + EXIT_INVALID_DECLARATION;
			}

			auto CLibraryName = Stringify(Value.substr(0, Offset1)).Trim().R();
			auto CSymbolName = Stringify(Value.substr(Offset1 + 1, Offset2 - Offset1 - 1)).Trim().R();
			auto Declaration = Stringify(Value.substr(Offset2 + 1)).Trim().R();
			if (CLibraryName.empty() || CSymbolName.empty() || Declaration.empty())
			{
				VI_ERR("invalid C library symbol declaration");
				return JUMP_CODE + EXIT_INVALID_DECLARATION;
			}

			auto& Data = Config.Symbols[CLibraryName];
			Data.first = CSymbolName;
			Data.second = Declaration;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-c, --compile", "save gz compressed compiled bytecode to a file near script file", [this](const String&)
		{
			Config.SaveByteCode = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-l, --load", "load gz compressed compiled bytecode and execute it as normal", [this](const String&)
		{
			Config.LoadByteCode = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-a, --app", "initialize only essentials for non-gui scripts", [this](const String&)
		{
			Config.EssentialsOnly = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-csymbols", "disable system module imports", [this](const String&)
		{
			Config.Modules = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-clibraries", "disable C library and external module imports", [this](const String&)
		{
			Config.CLibraries = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-csymbols", "disable C library symbolic imports", [this](const String&)
		{
			Config.CSymbols = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-files", "disable file imports", [this](const String&)
		{
			Config.Files = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-json", "disable json imports", [this](const String&)
		{
			Config.Files = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
	}
	void AddCommand(const String& Name, const String& Description, const CommandCallback& Callback)
	{
		Descriptions[Name] = Description;
		for (auto& Command : Stringify(&Name).Split(','))
		{
			auto Naming = Stringify(Command).Trim().Replace("-", "").R();
			auto& Data = Commands[Naming];
			Data.Callback = Callback;
			Data.Description = Description;
		}
	}
	void PrintIntroduction()
	{
		std::cout << "Welcome to Mavi.as v" << (uint32_t)Mavi::MAJOR_VERSION << "." << (uint32_t)Mavi::MINOR_VERSION << "." << (uint32_t)Mavi::PATCH_VERSION << " [" << Mavi::Library::GetCompiler() << " on " << Mavi::Library::GetPlatform() << "]" << std::endl;
		std::cout << "Run \"" << (Config.Interactive ? ".help" : (Config.Debug ? "help" : "vi --help")) << "\" for more information";
		if (Config.Interactive)
			std::cout << " (loaded " << VM->GetSubmodules().size() << " modules)";
		std::cout << std::endl;
	}
	void PrintHelp()
	{
		size_t Max = 0;
		for (auto& Next : Descriptions)
		{
			if (Next.first.size() > Max)
				Max = Next.first.size();
		}

		std::cout << "Usage: vi [options...]\n";
		std::cout << "       vi [options...] -f [file.as] [args...]\n\n";
		std::cout << "Options and flags:\n";
		for (auto& Next : Descriptions)
		{
			size_t Spaces = Max - Next.first.size();
			std::cout << "    " << Next.first;
			for (size_t i = 0; i < Spaces; i++)
				std::cout << " ";
			std::cout << " - " << Next.second << "\n";
		}
	}
	void ListenForSignals()
	{
		static Mavias* Instance = this;
		signal(SIGINT, [](int) { Instance->Interrupt(); });
		signal(SIGTERM, [](int) { Instance->Shutdown(); });
		signal(SIGABRT, [](int) { Instance->Abort("abort"); });
		signal(SIGFPE, [](int) { Instance->Abort("division by zero"); });
		signal(SIGILL, [](int) { Instance->Abort("illegal instruction"); });
		signal(SIGSEGV, [](int) { Instance->Abort("segmentation fault"); });
#ifdef VI_UNIX
		signal(SIGPIPE, SIG_IGN);
		signal(SIGCHLD, SIG_IGN);
#endif
	}
};

int main(int argc, char* argv[])
{
	Mavias* Instance = new Mavias(argc, argv);
    Mavi::Initialize(Instance->Config.EssentialsOnly ? (size_t)Mavi::Preset::App :(size_t)Mavi::Preset::Game);
	int ExitCode = Instance->Dispatch();
	delete Instance;
	Mavi::Uninitialize();

	return ExitCode;
}