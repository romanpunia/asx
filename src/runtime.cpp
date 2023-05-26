#include <signal.h>
#include "runtime.h"

class Mavias
{
private:
	OrderedMap<String, String, std::greater<String>> Descriptions;
	UnorderedMap<String, ProgramCommand> Commands;
	UnorderedMap<String, uint32_t> Settings;
	ProgramContext Contextual;
	ProgramEntrypoint Entrypoint;
	ProgramConfig Config;
	Function Terminate;
	VirtualMachine* VM;
	Compiler* Unit;

public:
	Mavias(int ArgsCount, char** Args) : Contextual(ArgsCount, Args), Terminate(nullptr), VM(nullptr), Unit(nullptr)
	{
		AddDefaultCommands();
		AddDefaultSettings();
		ListenForSignals();
		Config.EssentialsOnly = !Contextual.Params.Has("graphics", "g");
#ifndef NDEBUG1
		OS::Directory::SetWorking(OS::Directory::GetModule().c_str());
#endif
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

		VM = new VirtualMachine();
		Multiplexer::Create();

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

		if (!Config.Interactive && Contextual.Addon.empty() && Contextual.Program.empty())
		{
			Config.Interactive = true;
			if (Contextual.Params.Base.size() > 1)
			{
				VI_ERR("provide a path to existing script file");
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}
		}

		if (!Contextual.Addon.empty())
		{
			auto Time = GetTime();
			int ExitCode = BuilderCreateAddon();
			if (ExitCode == JUMP_CODE + EXIT_OK)
				std::cout << "Initialized addon directory: " << Contextual.Addon << std::endl;
			return ExitCode;
		}
		else if (Config.Interactive)
		{
			if (Contextual.Path.empty())
				Contextual.Path = OS::Directory::GetWorking();

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

		int Code = ConfigureEngine(Config, Contextual, VM);
		if (Code != 0)
			return Code;

		OS::Directory::SetWorking(OS::Path::GetDirectory(Contextual.Path.c_str()).c_str());
		if (Config.Debug)
			VM->SetDebugger(new DebuggerContext());

		Unit = VM->CreateCompiler();
		Unit->SetIncludeCallback(std::bind(&Mavias::BuilderImportAddon, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		if (Unit->Prepare(Contextual.Module) < 0)
		{
			VI_ERR("cannot prepare <%s> module scope", Contextual.Module);
			return JUMP_CODE + EXIT_PREPARE_FAILURE;
		}

		if (Config.Interactive)
		{
			String Data, Multidata;
			Data.reserve(1024 * 1024);
			VM->ImportSystemAddon("std");
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
					if (Unit->ExecuteScoped(Data, "any@").Get() == (int)Activation::Finished)
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
		else if (!Config.LoadByteCode)
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
		else if (Config.Dependencies)
		{
			PrintDependencies();
			return JUMP_CODE + EXIT_OK;
		}
		else if (Config.Update)
			return BuilderPullAddons();

		Function Main = GetEntrypoint(Contextual, Entrypoint, Unit);
		if (!Main.IsValid())
			return JUMP_CODE + EXIT_ENTRYPOINT_FAILURE;

		if (!Contextual.Output.empty())
		{
			auto Time = GetTime();
			int ExitCode = BuilderCreateExecutable();
			if (ExitCode == JUMP_CODE + EXIT_OK)
				std::cout << "Built binaries directory: " << Contextual.Output << "bin" << std::endl;
			return ExitCode;
		}

		if (Config.Debug)
			PrintIntroduction();

		if (Config.EssentialsOnly)
		{
			if (VM->HasSystemAddon("std/graphics") || VM->HasSystemAddon("std/audio"))
				VI_WARN("program includes disabled graphics/audio features: consider using -g option");
		}
		else if (!VM->HasSystemAddon("std/graphics") && !!VM->HasSystemAddon("std/audio"))
			VI_WARN("program does not include loaded graphics/audio features: consider removing -g option");

		ImmediateContext* Context = Unit->GetContext();
		Context->SetExceptionCallback([](ImmediateContext* Context)
		{
			if (!Context->WillExceptionBeCaught())
			{
				VI_ERR("program has failed to catch an exception; killed");
				std::exit(JUMP_CODE + EXIT_RUNTIME_FAILURE);
			}
		});
		Terminate = Unit->GetModule().GetFunctionByDecl(Entrypoint.Terminate);

		TypeInfo Type = VM->GetTypeInfoByDecl("array<string>@");
		Bindings::Array* ArgsArray = Type.IsValid() ? Bindings::Array::Compose<String>(Type.GetTypeInfo(), Contextual.Args) : nullptr;
		Context->Execute(Main, [&Main, ArgsArray](ImmediateContext* Context)
		{
			if (Main.GetArgsCount() > 0)
				Context->SetArgObject(0, ArgsArray);
		}).Wait();

		int ExitCode = Main.GetReturnTypeId() == (int)TypeId::VOIDF ? 0 : (int)Context->GetReturnDWord();
		if (ArgsArray != nullptr)
			VM->ReleaseObject(ArgsArray, Type);
	
		AwaitContext(Queue, VM, Context);
		return ExitCode;
	}
	void Shutdown()
	{
		{
			if (Terminate.IsValid() && Unit->GetContext()->Execute(Terminate, nullptr).Get() == 0)
			{
				Terminate = nullptr;
				VI_DEBUG("graceful shutdown using [%s call]", Entrypoint.Terminate);
				goto GracefulShutdown;
			}

			auto* App = Application::Get();
			if (App != nullptr && App->GetState() == ApplicationState::Active)
			{
				App->Stop();
				VI_DEBUG("graceful shutdown using [application stop]");
				goto GracefulShutdown;
			}

			auto* Queue = Schedule::Get();
			if (Queue->IsActive())
			{
				Queue->Stop();
				VI_DEBUG("graceful shutdown using [scheduler stop]");
				goto GracefulShutdown;
			}

			VI_DEBUG("forcing shutdown using [kill]");
			return std::exit(JUMP_CODE + EXIT_KILL);
		}
	GracefulShutdown:
		ListenForSignals();
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
	bool WantsAllFeatures()
	{
		return !Config.EssentialsOnly;
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
			String Directory = OS::Directory::GetWorking();
			size_t Index = 0; bool FileFlag = false;

			for (size_t i = 0; i < Contextual.Args.size(); i++)
			{
				auto& Value = Contextual.Args[i];
				if (!FileFlag)
				{
					FileFlag = (Value == "-f" || Value == "--file");
					continue;
				}

				auto File = OS::Path::Resolve(Value, Directory, true);
				if (OS::File::State(File, &Contextual.File) && !Contextual.File.IsDirectory)
				{
					Contextual.Path = File;
					Index = i;
					break;
				}

				File = OS::Path::Resolve(Value + (Config.LoadByteCode ? ".as.gz" : ".as"), Directory, true);
				if (OS::File::State(File, &Contextual.File) && !Contextual.File.IsDirectory)
				{
					Contextual.Path = File;
					Index = i;
					break;
				}
			}

			if (!Contextual.File.IsExists)
			{
				VI_ERR("path <%s> does not exist", Path.c_str());
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}

			Contextual.Registry = OS::Path::GetDirectory(Contextual.Path.c_str());
			if (Contextual.Registry == Contextual.Path)
			{
				Contextual.Path = Directory + Contextual.Path;
				Contextual.Registry = OS::Path::GetDirectory(Contextual.Path.c_str());
			}

			Contextual.Args.erase(Contextual.Args.begin(), Contextual.Args.begin() + Index);
			Contextual.Module = OS::Path::GetFilename(Contextual.Path.c_str());
			Contextual.Program = OS::File::ReadAsString(Contextual.Path.c_str());
			Contextual.Registry += "addons";
			Contextual.Registry += VI_SPLITTER;
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
		AddCommand("-i, --interactive", "enable interactive mode", [this](const String )
		{
			Config.Interactive = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-o, --output", "directory where to build an executable from source code", [this](const String& Path)
		{
			FileEntry File;
			Contextual.Output = (Path == "." ? OS::Directory::GetWorking() : OS::Path::Resolve(Path, OS::Directory::GetWorking(), true));
			if (!Contextual.Output.empty() && (Contextual.Output.back() == '/' || Contextual.Output.back() == '\\'))
				Contextual.Output.erase(Contextual.Output.end() - 1);

			if (!Contextual.Name.empty())
				Contextual.Output += VI_SPLITTER + Contextual.Name + VI_SPLITTER;
			else
				Contextual.Output += VI_SPLITTER;

			if (!OS::File::State(Contextual.Output, &File))
			{
				OS::Directory::Patch(Contextual.Output);
				return JUMP_CODE + EXIT_CONTINUE;
			}

			if (File.IsDirectory)
				return JUMP_CODE + EXIT_CONTINUE;

			VI_ERR("output path <%s> must be a directory", Path.c_str());
			return JUMP_CODE + EXIT_INPUT_FAILURE;
		});
		AddCommand("-n, --name", "set a CMake name for output target", [this](const String& Name)
		{
			Contextual.Name = Name;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-s, --system", "import system addon(s) by name [expects: plus(+) separated list]", [this](const String& Value)
		{
			for (auto& Item : Stringify(&Value).Split('+'))
				Config.SystemAddons.push_back(Item);

			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-a, --addon", "import external addon(s) by path [expects: plus(+) separated list]", [this](const String& Value)
		{
			for (auto& Item : Stringify(&Value).Split('+'))
				Config.Libraries.emplace_back(std::make_pair(Item, true));

			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-cl, --clib", "import clibrary(ies) by path [expects: plus(+) separated list]", [this](const String& Value)
		{
			for (auto& Item : Stringify(&Value).Split('+'))
				Config.Libraries.emplace_back(std::make_pair(Item, false));

			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-cf, --cfunction", "import clibrary function by declaration [expects: clib_name:cfunc_name=asfunc_decl]", [this](const String& Value)
		{
			size_t Offset1 = Value.find(':');
			if (Offset1 == std::string::npos)
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", Value.c_str());
				return JUMP_CODE + EXIT_INVALID_DECLARATION;
			}

			size_t Offset2 = Value.find('=', Offset1);
			if (Offset2 == std::string::npos)
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", Value.c_str());
				return JUMP_CODE + EXIT_INVALID_DECLARATION;
			}

			auto CLibraryName = Stringify(Value.substr(0, Offset1)).Trim().R();
			auto CFunctionName = Stringify(Value.substr(Offset1 + 1, Offset2 - Offset1 - 1)).Trim().R();
			auto Declaration = Stringify(Value.substr(Offset2 + 1)).Trim().R();
			if (CLibraryName.empty() || CFunctionName.empty() || Declaration.empty())
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", Value.c_str());
				return JUMP_CODE + EXIT_INVALID_DECLARATION;
			}

			auto& Data = Config.Functions[CLibraryName];
			Data.first = CFunctionName;
			Data.second = Declaration;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-ps, --preserve-sources", "save source code in memory to append to runtime exception stack-traces", [this](const String&)
		{
			Config.SaveSourceCode = true;
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
		AddCommand("-g, --graphics", "initialize graphics and audio for scripts (takes more time to startup)", [this](const String&)
		{
			Config.EssentialsOnly = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-u, --use", "set virtual machine property (expects: prop_name:prop_value)", [this](const String& Value)
		{
			auto Args = Stringify(&Value).Split(':');
			if (Args.size() != 2)
			{
				VI_ERR("invalid property declaration <%s>", Value.c_str());
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}

			auto It = Settings.find(Stringify(&Args[0]).Trim().R());
			if (It == Settings.end())
			{
				VI_ERR("invalid property name <%s>", Args[0].c_str());
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}

			Stringify Data(&Args[1]);
			Data.Trim().ToLower();

			if (Data.Empty())
			{
			InputFailure:
				VI_ERR("property value <%s>: %s", Args[0].c_str(), Args[1].empty() ? "?" : Args[1].c_str());
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}
			else if (!Data.HasInteger())
			{
				if (Args[1] == "on" || Args[1] == "true")
					VM->SetProperty((Features)It->second, 1);
				else if (Args[1] == "off" || Args[1] == "false")
					VM->SetProperty((Features)It->second, 1);
				else
					goto InputFailure;
			}

			VM->SetProperty((Features)It->second, (size_t)Data.ToUInt64());
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-init, --init", "initialize an addon template in given directory (expects: [native|vm]:relpath)", [this](const String& Value)
		{
			size_t Where = Value.find(':');
			if (Where == std::string::npos)
			{
				VI_ERR("addon initialization expects <mode:path> format: <%s> is invalid", Value.c_str());
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}

			String Path = Value.substr(Where + 1);
			if (Path.empty())
			{
				VI_ERR("addon initialization expects <mode:path> format: path must not be empty");
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}

			Contextual.Mode = Value.substr(0, Where);
			if (Contextual.Mode != "native" && Contextual.Mode != "vm")
			{
				VI_ERR("addon initialization expects <mode:path> format: mode <%s> is invalid, [native|vm] expected", Contextual.Mode.c_str());
				return JUMP_CODE + EXIT_INPUT_FAILURE;
			}

			FileEntry File;
			Contextual.Addon = (Path == "." ? OS::Directory::GetWorking() : OS::Path::Resolve(Path, OS::Directory::GetWorking(), true));
			if (!Contextual.Addon.empty() && (Contextual.Addon.back() == '/' || Contextual.Addon.back() == '\\'))
				Contextual.Addon.erase(Contextual.Addon.end() - 1);

			if (!Contextual.Name.empty())
				Contextual.Addon += VI_SPLITTER + Contextual.Name + VI_SPLITTER;
			else
				Contextual.Addon += VI_SPLITTER;

			if (!OS::File::State(Contextual.Addon, &File))
			{
				OS::Directory::Patch(Contextual.Addon);
				return JUMP_CODE + EXIT_CONTINUE;
			}

			if (File.IsDirectory)
				return JUMP_CODE + EXIT_CONTINUE;

			VI_ERR("addon path <%s> must be a directory", Path.c_str());
			return JUMP_CODE + EXIT_INPUT_FAILURE;
		});
		AddCommand("--update", "update locally installed addons", [this](const String& Value)
		{
			Config.Update = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--fast", "will use single directory for all CMake addon builds (reuses cache, similar addon names will cause conflicts)", [this](const String& Value)
		{
			Config.FastBuilds = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--uses, --settings, --properties", "show virtual machine properties message", [this](const String&)
		{
			PrintProperties();
			return JUMP_CODE + EXIT_OK;
		});
		AddCommand("--deps, --install, --dependencies", "install and show dependencies message", [this](const String&)
		{
			Config.Dependencies = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-addons", "disable system addon imports", [this](const String&)
		{
			Config.Addons = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-clibraries", "disable clibrary and external addon imports", [this](const String&)
		{
			Config.CLibraries = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-cfunctions", "disable clibrary cfunction imports", [this](const String&)
		{
			Config.CFunctions = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-files", "disable file imports", [this](const String&)
		{
			Config.Files = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("--no-remotes", "disable remote imports", [this](const String&)
		{
			Config.Remotes = false;
			return JUMP_CODE + EXIT_CONTINUE;
		});
	}
	void AddDefaultSettings()
	{
		Settings["default_stack_size"] = (uint32_t)Features::INIT_STACK_SIZE;
		Settings["default_callstack_size"] = (uint32_t)Features::INIT_CALL_STACK_SIZE;
		Settings["callstack_size"] = (uint32_t)Features::MAX_CALL_STACK_SIZE;
		Settings["stack_size"] = (uint32_t)Features::MAX_STACK_SIZE;
		Settings["string_encoding"] = (uint32_t)Features::STRING_ENCODING;
		Settings["script_encoding_utf8"] = (uint32_t)Features::SCRIPT_SCANNER;
		Settings["no_globals"] = (uint32_t)Features::DISALLOW_GLOBAL_VARS;
		Settings["warnings"] = (uint32_t)Features::COMPILER_WARNINGS;
		Settings["unicode"] = (uint32_t)Features::ALLOW_UNICODE_IDENTIFIERS;
		Settings["nested_calls"] = (uint32_t)Features::MAX_NESTED_CALLS;
		Settings["unsafe_references"] = (uint32_t)Features::ALLOW_UNSAFE_REFERENCES;
		Settings["optimized_bytecode"] = (uint32_t)Features::OPTIMIZE_BYTECODE;
		Settings["copy_scripts"] = (uint32_t)Features::COPY_SCRIPT_SECTIONS;
		Settings["character_literals"] = (uint32_t)Features::USE_CHARACTER_LITERALS;
		Settings["multiline_strings"] = (uint32_t)Features::ALLOW_MULTILINE_STRINGS;
		Settings["implicit_handles"] = (uint32_t)Features::ALLOW_IMPLICIT_HANDLE_TYPES;
		Settings["suspends"] = (uint32_t)Features::BUILD_WITHOUT_LINE_CUES;
		Settings["init_globals"] = (uint32_t)Features::INIT_GLOBAL_VARS_AFTER_BUILD;
		Settings["require_enum_scope"] = (uint32_t)Features::REQUIRE_ENUM_SCOPE;
		Settings["jit_instructions"] = (uint32_t)Features::INCLUDE_JIT_INSTRUCTIONS;
		Settings["accessor_mode"] = (uint32_t)Features::PROPERTY_ACCESSOR_MODE;
		Settings["array_template_message"] = (uint32_t)Features::EXPAND_DEF_ARRAY_TO_TMPL;
		Settings["automatic_gc"] = (uint32_t)Features::AUTO_GARBAGE_COLLECT;
		Settings["automatic_constructors"] = (uint32_t)Features::ALWAYS_IMPL_DEFAULT_CONSTRUCT;
		Settings["value_assignment_to_references"] = (uint32_t)Features::DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE;
		Settings["named_args_mode"] = (uint32_t)Features::ALTER_SYNTAX_NAMED_ARGS;
		Settings["integer_division_mode"] = (uint32_t)Features::DISABLE_INTEGER_DIVISION;
		Settings["no_empty_list_elements"] = (uint32_t)Features::DISALLOW_EMPTY_LIST_ELEMENTS;
		Settings["private_is_protected"] = (uint32_t)Features::PRIVATE_PROP_AS_PROTECTED;
		Settings["heredoc_trim_mode"] = (uint32_t)Features::HEREDOC_TRIM_MODE;
		Settings["generic_auto_handles_mode"] = (uint32_t)Features::GENERIC_CALL_MODE;
		Settings["ignore_shared_interface_duplicates"] = (uint32_t)Features::IGNORE_DUPLICATE_SHARED_INTF;
		Settings["ignore_debug_output"] = (uint32_t)Features::NO_DEBUG_OUTPUT;
	}
	void AddCommand(const String& Name, const String& Description, const CommandCallback& Callback)
	{
		Descriptions[Name] = Description;
		for (auto& Command : Stringify(&Name).Split(','))
		{
			auto Naming = Stringify(Command).Trim().R();
			while (!Naming.empty() && Naming.front() == '-')
				Naming.erase(Naming.begin());
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
			std::cout << " (loaded " << VM->GetExposedAddons().size() << " addons)";
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
	void PrintProperties()
	{
		for (auto& Item : Settings)
		{
			Stringify Name(&Item.first);
			size_t Value = VM->GetProperty((Features)Item.second);
			std::cout << "  " << Item.first << ": ";
			if (Name.EndsWith("mode"))
				std::cout << "mode " << Value;
			else if (Name.EndsWith("size"))
				std::cout << (Value > 0 ? ToString(Value) : "unlimited");
			else if (Value == 0)
				std::cout << "OFF";
			else if (Value == 1)
				std::cout << "ON";
			else
				std::cout << Value;
			std::cout << "\n";
		}
	}
	void PrintDependencies()
	{
		auto Exposes = VM->GetExposedAddons();
		if (!Exposes.empty())
		{
			std::cout << "  local dependencies list:" << std::endl;
			for (auto& Item : Exposes)
				std::cout << "    " << Stringify(&Item).Replace(":", ": ").R() << std::endl;
		}

		if (!Contextual.Addons.empty())
		{
			std::cout << "  remote dependencies list:" << std::endl;
			for (auto& Item : Contextual.Addons)
				std::cout << "    " << Item << ": " << GetBuilderAddonTargetLibrary(Item, nullptr) << std::endl;
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
	IncludeType BuilderImportAddon(Preprocessor* Base, const IncludeResult& File, String& Output)
	{
		if (File.Module.empty() || File.Module.front() != '@')
			return IncludeType::Unchanged;

		if (!Config.CFunctions || !Config.Remotes)
		{
			VI_ERR("cannot import addon <%s> from remote repository: not allowed", File.Module.c_str());
			return IncludeType::Error;
		}

		IncludeType Status;
		if (IsBuilderAddonCached(File.Module))
			Status = BuilderFetchAddonCache(File.Module, Output);
		else
			Status = BuilderCreateAddonCache(File.Module, Output);

		Contextual.Addons.insert(File.Module);
		return Status;
	}
	IncludeType BuilderCreateAddonCache(const String& Name, String& Output)
	{
		String LocalTarget = Contextual.Registry + Name + VI_SPLITTER, RemoteTarget = Name.substr(1);
		if (IsBuilderDirectoryEmpty(LocalTarget) && BuilderExecuteGit("git clone " REPOSITORY_SOURCE + RemoteTarget + " " + LocalTarget) != 0)
		{
			VI_ERR("addon <%s> does not seem to be available at remote repository: <%s>", RemoteTarget.c_str());
			return IncludeType::Error;
		}

		UPtr<Schema> Info = GetBuilderAddonInfo(Name);
		if (!Info)
		{
			VI_ERR("addon <%s> does not seem to have a valid " FILE_ADDON " file", Name.c_str());
			return IncludeType::Error;
		}

		String Type = Info->GetVar("type").GetBlob();
		if (Type == "native")
		{
			if (IsBuilderAddonsCacheRegistryEmpty() && BuilderCreateAddonCacheRegistry() != JUMP_CODE + EXIT_OK)
			{
				VI_ERR("addon <%s> cannot be created: global target cannot be built", Name.c_str());
				return IncludeType::Error;
			}

			if (BuilderCreateAddonLibrary(LocalTarget, GetBuilderDirectory(LocalTarget)) != JUMP_CODE + EXIT_OK)
			{
				VI_ERR("addon <%s> cannot be created: final target cannot be built", Name.c_str());
				return IncludeType::Error;
			}

			String Path = GetBuilderAddonTarget(Name);
			return VM->ImportAddon(Path) ? IncludeType::Virtual : IncludeType::Error;
		}
		else if (Type == "vm")
		{
			Stringify Index(Info->GetVar("index").GetBlob());
			if (Index.Empty() || !Index.EndsWith(".as") || Index.FindOf("/\\").Found)
			{
				VI_ERR("addon <%s> cannot be created: index file <%s> is not valid", Name.c_str(), Index.Get());
				return IncludeType::Error;
			}

			String Path = LocalTarget + Index.R();
			if (!OS::File::IsExists(Path.c_str()))
			{
				VI_ERR("addon <%s> cannot be created: index file cannot be found", Name.c_str());
				return IncludeType::Error;
			}

			Output = OS::File::ReadAsString(Path.c_str());
			return IncludeType::Preprocess;
		}

		VI_ERR("addon <%s> does not seem to have a valid " FILE_ADDON " file: type <%s> is not recognized", Name.c_str(), Type.c_str());
		return IncludeType::Error;
	}
	IncludeType BuilderFetchAddonCache(const String& Name, String& Output)
	{
		UPtr<Schema> Info = GetBuilderAddonInfo(Name);
		if (!Info)
		{
			VI_ERR("addon <%s> does not seem to have a valid " FILE_ADDON " file", Name.c_str());
			return IncludeType::Error;
		}

		String Type = Info->GetVar("type").GetBlob();
		if (Type == "native")
		{
			String Path = GetBuilderAddonTarget(Name);
			return VM->ImportAddon(Path) ? IncludeType::Virtual : IncludeType::Error;
		}
		else if (Type == "vm")
		{
			String Path = Contextual.Registry + Name + VI_SPLITTER + Info->GetVar("index").GetBlob();
			if (!OS::File::IsExists(Path.c_str()))
			{
				VI_ERR("addon <%s> cannot be imported: index file cannot be found", Name.c_str());
				return IncludeType::Error;
			}

			Output = OS::File::ReadAsString(Path.c_str());
			return IncludeType::Preprocess;
		}

		VI_ERR("addon <%s> does not seem to have a valid " FILE_ADDON " file: type <%s> is not recognized", Name.c_str(), Type.c_str());
		return IncludeType::Error;
	}
	int BuilderCreateAddonCacheRegistry()
	{
		String GlobalTarget = Contextual.Registry + "mavi";
		if (!IsBuilderDirectoryEmpty(GlobalTarget))
			return JUMP_CODE + EXIT_OK;

		if (BuilderExecuteGit("git clone --recursive " REPOSITORY_TARGET_MAVI " " + GlobalTarget) != 0)
		{
			VI_ERR("cannot clone addons repository target");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		return JUMP_CODE + EXIT_OK;
	}
	int BuilderCreateAddonLibrary(const String& SourcesDirectory, const String& BuildDirectory)
	{
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		String ConfigureCommand = Form("cmake -S %s -B %s -DVI_DIRECTORY=%smavi", SourcesDirectory.c_str(), BuildDirectory.c_str(), Contextual.Registry.c_str()).R();
#else
		String ConfigureCommand = Form("cmake -S %s -B %s -DVI_DIRECTORY=%smavi -DCMAKE_BUILD_TYPE=%s", SourcesDirectory.c_str(), BuildDirectory.c_str(), Contextual.Registry.c_str(), GetBuilderBuildType(true)).R();
#endif
		if (BuilderExecuteCMake(ConfigureCommand) != 0)
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		String BuildCommand = Form("cmake --build %s --config %s", BuildDirectory.c_str(), GetBuilderBuildType(true)).R();
#else
		String BuildCommand = Form("cmake --build %s", BuildDirectory.c_str()).R();
#endif
		if (BuilderExecuteCMake(BuildCommand) != 0)
			return JUMP_CODE + EXIT_COMMAND_FAILURE;

		return JUMP_CODE + EXIT_OK;
	}
	int BuilderCreateExecutable()
	{
		if (IsBuilderDirectoryEmpty(Contextual.Output) && BuilderExecuteGit("git clone --recursive " REPOSITORY_TEMPLATE_EXECUTABLE " " + Contextual.Output) != 0)
		{
			VI_ERR("cannot clone executable repository");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		if (BuilderGenerate(Contextual.Output, false) != 0)
		{
			VI_ERR("cannot generate the template:\n\tmake sure application has file read/write permissions");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		if (BuilderAppendByteCode(Contextual.Output + "src/program.hex") != 0)
		{
			VI_ERR("cannot embed the byte code:\n\tmake sure application has file read/write permissions");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		if (BuilderAppendDependencies(Contextual.Output + "bin/") != 0)
		{
			VI_ERR("cannot embed the dependencies:\n\tmake sure application has file read/write permissions");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		String ConfigureCommand = Form("cmake -S %s -B %smake", Contextual.Output.c_str(), Contextual.Output.c_str()).R();
#else
		String ConfigureCommand = Form("cmake -S %s -B %smake -DCMAKE_BUILD_TYPE=%s", Contextual.Output.c_str(), Contextual.Output.c_str(), GetBuilderBuildType(false)).R();
#endif
		if (BuilderExecuteCMake(ConfigureCommand) != 0)
		{
#ifdef VI_MICROSOFT
			VI_ERR("cannot configure an executable repository:\n\tmake sure you vcpkg installed and VCPKG_ROOT env is set");
#else
			VI_ERR("cannot configure an executable repository:\n\tmake sure you have all dependencies installed");
#endif
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		String BuildCommand = Form("cmake --build %smake --config %s", Contextual.Output.c_str(), GetBuilderBuildType(false)).R();
#else
		String BuildCommand = Form("cmake --build %smake", Contextual.Output.c_str()).R();
#endif
		if (BuilderExecuteCMake(BuildCommand) != 0)
		{
			VI_ERR("cannot build an executable repository");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		return JUMP_CODE + EXIT_OK;
	}
	int BuilderCreateAddon()
	{
		if (Contextual.Mode == "vm")
		{
			Schema* Info = Var::Set::Object();
			Info->Set("name", Var::String(Contextual.Name));
			Info->Set("type", Var::String(Contextual.Mode));
			Info->Set("index", Var::String(FILE_INDEX));
			Info->Set("runtime", Var::String(GetViVersion()));
			Info->Set("version", Var::String("1.0.0"));

			String Offset, Data;
			Schema::ConvertToJSON(Info, [&Offset, &Data](VarForm Pretty, const char* Buffer, size_t Length)
			{
				if (Buffer != nullptr && Length > 0)
					Data.append(Buffer, Length);

				switch (Pretty)
				{
					case VarForm::Tab_Decrease:
						Offset.erase(Offset.end() - 1);
						break;
					case VarForm::Tab_Increase:
						Offset.append(1, '\t');
						break;
					case VarForm::Write_Space:
						Data.append(" ");
						break;
					case VarForm::Write_Line:
						Data.append("\n");
						break;
					case VarForm::Write_Tab:
						Data.append(Offset);
						break;
					default:
						break;
				}
			});
			VI_RELEASE(Info);

			OS::Directory::Patch(Contextual.Addon);
			OS::File::Write(Contextual.Addon + FILE_INDEX, String());
			if (!OS::File::Write(Contextual.Addon + FILE_ADDON, Data))
			{
				VI_ERR("cannot generate the template:\n\tmake sure application has file read/write permissions");
				return false;
			}
		}
		else if (Contextual.Mode == "native")
		{
			if (!IsBuilderDirectoryEmpty(Contextual.Addon))
			{
				VI_ERR("cannot clone addon repository:\n\ttarget directory is not empty: %s", Contextual.Addon.c_str());
				return JUMP_CODE + EXIT_ENTRYPOINT_FAILURE;
			}

			if (BuilderExecuteGit("git clone --recursive " REPOSITORY_TEMPLATE_ADDON " " + Contextual.Addon) != 0)
			{
				VI_ERR("cannot clone executable repository");
				return JUMP_CODE + EXIT_COMMAND_FAILURE;
			}

			String GitDirectory = Contextual.Addon + ".git";
			OS::Directory::Remove(GitDirectory.c_str());

			String MakeDirectory = Contextual.Addon + "make";
			OS::Directory::Create(MakeDirectory.c_str());
			
			if (BuilderGenerate(Contextual.Addon, true) != 0)
			{
				VI_ERR("cannot generate the template:\n\tmake sure application has file read/write permissions");
				return JUMP_CODE + EXIT_COMMAND_FAILURE;
			}

			return JUMP_CODE + EXIT_OK;
		}

		return JUMP_CODE + EXIT_INPUT_FAILURE;
	}
	int BuilderPullAddons()
	{
		if (Contextual.Registry.empty())
		{
			VI_ERR("provide entrypoint file to pull addons");
			return JUMP_CODE + EXIT_INPUT_FAILURE;
		}

		Vector<FileEntry> Entries;
		if (!OS::Directory::Scan(Contextual.Registry.c_str(), &Entries) || Entries.empty())
			return JUMP_CODE + EXIT_OK;

		auto Pull = [this](const String& Path) { return BuilderExecuteGit("cd \"" + Path + "\" && git pull") == 0; };
		for (auto& File : Entries)
		{
			if (!File.IsDirectory || File.Path.empty() || File.Path.front() == '.')
				continue;

			if (File.Path.front() == '@')
			{
				Vector<FileEntry> Addons;
				String RepositoriesPath = Contextual.Registry + File.Path + VI_SPLITTER;
				if (!OS::Directory::Scan(RepositoriesPath.c_str(), &Addons) || Addons.empty())
					continue;

				for (auto& Addon : Addons)
				{
					if (Addon.IsDirectory && !Pull(RepositoriesPath + Addon.Path))
					{
						VI_ERR("cannot pull addon target repository: %s", File.Path.c_str());
						return JUMP_CODE + EXIT_COMMAND_FAILURE;
					}
				}
			}
			else if (!Pull(Contextual.Registry + File.Path))
			{
				VI_ERR("cannot pull addon source repository: %s", File.Path.c_str());
				return JUMP_CODE + EXIT_COMMAND_FAILURE;
			}
		}

		return JUMP_CODE + EXIT_OK;
	}
	int BuilderExecuteGit(const String& Command)
	{
		static int IsGitInstalled = -1;
		if (IsGitInstalled == -1)
		{
			std::cout << "> CHECK git:" << std::endl;
			if (OS::IsLogPretty())
				Console::Get()->ColorBegin(StdColor::Gray);

			IsGitInstalled = (int)(system("git") == COMMAND_GIT_EXIT_OK);
			if (OS::IsLogPretty())
				Console::Get()->ColorEnd();

			if (!IsGitInstalled)
			{
				VI_ERR("cannot find <git> program, please make sure it is installed");
				return JUMP_CODE + EXIT_COMMAND_FAILURE;
			}
		}

		return BuilderExecute(Command);
	}
	int BuilderExecuteCMake(const String& Command)
	{
		static int IsCMakeInstalled = -1;
		if (IsCMakeInstalled == -1)
		{
			std::cout << "> CHECK cmake:" << std::endl;
			if (OS::IsLogPretty())
				Console::Get()->ColorBegin(StdColor::Gray);

			IsCMakeInstalled = (int)(system("cmake") == COMMAND_CMAKE_EXIT_OK);
			if (OS::IsLogPretty())
				Console::Get()->ColorEnd();

			if (!IsCMakeInstalled)
			{
				VI_ERR("cannot find <cmake> program, please make sure it is installed");
				return JUMP_CODE + EXIT_COMMAND_FAILURE;
			}
		}

		return BuilderExecute(Command);
	}
	int BuilderExecute(const String& Command)
	{
		std::cout << "> " + Command << ":" << std::endl;
		if (OS::IsLogPretty())
			Console::Get()->ColorBegin(StdColor::Gray);

		ProcessStream* Stream = OS::Process::ExecuteReadOnly(Command);
		if (!Stream)
			return -1;

		bool NewLineEOF = false;
		size_t Size = Stream->ReadAll([&NewLineEOF](char* Buffer, size_t Size)
		{
			if (!Size)
				return;

			std::cout << String(Buffer, Size);
			NewLineEOF = Buffer[Size - 1] == '\r' || Buffer[Size - 1] == '\n';
		});

		if (NewLineEOF)
			std::cout << std::endl;

		if (OS::IsLogPretty())
			Console::Get()->ColorEnd();

		if (!Stream->Close())
			VI_ERR("cannot close a child process");

		int ExitCode = Stream->GetExitCode();
		VI_RELEASE(Stream);
		return ExitCode;
	}
	int BuilderGenerate(const String& Path, bool IsAddon)
	{
		Vector<FileEntry> Entries;
		if (!OS::Directory::Scan(Path, &Entries))
		{
			VI_ERR("cannot scan directory: %s", Path.c_str());
			return -1;
		}

		size_t TotalSize = 0;
		for (auto It = Entries.begin(); It != Entries.end();)
		{
			if (!It->Path.empty() && It->Path.front() != '.' && It->Path != "make" && It->Path != "bin" && It->Path != "deps" && It->Path.rfind(".codegen") == std::string::npos && It->Path.rfind(".hex") == std::string::npos)
			{
				if (!Path.empty() && Path.back() != '\\' && Path.back() != '/')
					It->Path = Path + VI_SPLITTER + It->Path;
				else
					It->Path = Path + It->Path;

				size_t Index = It->Path.rfind(".template");
				if (Index != std::string::npos)
				{
					String TargetPath = It->Path.substr(0, Index);
					if (OS::File::IsExists(TargetPath.c_str()))
					{
						OS::File::Remove(TargetPath.c_str());
						OS::File::Move(It->Path.c_str(), TargetPath.c_str());
						It = Entries.erase(It);
						continue;
					}
					else
					{
						OS::File::Move(It->Path.c_str(), TargetPath.c_str());
						It->Path = TargetPath;
					}
				}

				TotalSize += It->Size;
				++It;
			}
			else
				It = Entries.erase(It);
		}

		size_t CurrentSize = 0;
		for (auto& Item : Entries)
		{
			String TargetPath = Item.Path + ".codegen";
			CurrentSize += Item.Size;
			if (Item.IsDirectory)
			{
				int Status = BuilderGenerate(Item.Path, IsAddon);
				if (Status != 0)
					return Status;
				continue;
			}

			Stream* BaseFile = OS::File::Open(Item.Path, FileMode::Binary_Read_Only);
			if (!BaseFile)
			{
				VI_ERR("cannot open source file: %s", Item.Path.c_str());
				return -1;
			}

			Stream* TargetFile = OS::File::Open(TargetPath, FileMode::Binary_Write_Only);
			if (!TargetFile)
			{
				VI_ERR("cannot create target source file: %s", TargetPath.c_str());
				VI_RELEASE(BaseFile);
				return -1;
			}

			uint32_t Progress = (uint32_t)(100.0 * (double)CurrentSize / (double)TotalSize);
			BaseFile->ReadAll([this, TargetFile, &IsAddon](char* Buffer, size_t Size)
			{
				if (!Size)
					return;

				String Data(Buffer, Size);
				BuilderGenerateTemplate(Data, IsAddon);
				TargetFile->Write(Data.c_str(), Data.size());
			});
			VI_RELEASE(BaseFile);
			VI_RELEASE(TargetFile);

			if (!IsAddon)
			{
				String TempPath = Item.Path + ".template";
				OS::File::Move(Item.Path.c_str(), TempPath.c_str());
			}
			else
				OS::File::Remove(Item.Path.c_str());

			if (!OS::File::Move(TargetPath.c_str(), Item.Path.c_str()))
			{
				VI_ERR("cannot move target files:\n\tmove file from: %s\n\tmove file to: %s", TargetPath.c_str(), Item.Path.c_str());
				return -1;
			}
		}

		return 0;
	}
	int BuilderGenerateTemplate(String& Data, bool IsAddon)
	{
		static String ConfigSettingsArray;
		if (ConfigSettingsArray.empty())
		{
			for (auto& Item : Settings)
			{
				size_t Value = VM->GetProperty((Features)Item.second);
				ConfigSettingsArray += Form("{ (uint32_t)%i, (size_t)%" PRIu64 " }, ", Item.second, (uint64_t)Value).R();
			}
		}

		static String ConfigSystemAddonsArray;
		if (ConfigSystemAddonsArray.empty())
		{
			for (auto& Item : VM->GetSystemAddons())
			{
				if (Item.second.Exposed)
					ConfigSystemAddonsArray += Form("\"%s\", ", Item.first.c_str()).R();
			}
		}

		static String ConfigLibrariesArray;
		static String ConfigFunctionsArray;
		if (ConfigLibrariesArray.empty() && ConfigFunctionsArray.empty())
		{
			for (auto& Item : VM->GetCLibraries())
			{
				ConfigLibrariesArray += Form("{ \"%s\", %s }, ", Item.first.c_str(), Item.second.IsAddon ? "true" : "false").R();
				if (Item.second.IsAddon)
					continue;

				for (auto& Function : Item.second.Functions)
					ConfigFunctionsArray += Form("{ \"%s\", { \"%s\", \"%s\" } }, ", Item.first.c_str(), Function.first.c_str()).R();
			}
		}

		static String ViFeatures;
		if (ViFeatures.empty())
		{
			Vector<std::pair<String, bool>> Features =
			{
				{ "BINDINGS", Mavi::Library::HasBindings() },
				{ "FAST_MEMORY", Mavi::Library::HasFastMemory() },
				{ "FCTX", Mavi::Library::HasFContext() && !IsAddon },
				{ "SIMD", Mavi::Library::HasSIMD() && !IsAddon },
				{ "ASSIMP", Mavi::Library::HasAssimp() && IsBuilderUsingEngine() && !IsAddon },
				{ "FREETYPE", Mavi::Library::HasFreeType() && IsBuilderUsingGUI() && !IsAddon },
				{ "GLEW", Mavi::Library::HasGLEW() && IsBuilderUsingGraphics() && !IsAddon },
				{ "OPENAL", Mavi::Library::HasOpenAL() && IsBuilderUsingAudio() && !IsAddon },
				{ "OPENGL", Mavi::Library::HasOpenGL() && IsBuilderUsingGraphics() && !IsAddon },
				{ "OPENSSL", Mavi::Library::HasOpenSSL() && IsBuilderUsingCrypto() && !IsAddon },
				{ "ZLIB", Mavi::Library::HasOpenSSL() && IsBuilderUsingCompression() && !IsAddon },
				{ "SDL2", Mavi::Library::HasSDL2() && IsBuilderUsingGraphics() && !IsAddon },
				{ "POSTGRESQL", Mavi::Library::HasPostgreSQL() && IsBuilderUsingPostgreSQL() && !IsAddon },
				{ "MONGOC", Mavi::Library::HasMongoDB() && IsBuilderUsingMongoDB() && !IsAddon },
				{ "SPIRV", Mavi::Library::HasSPIRV() && IsBuilderUsingGraphics() && !IsAddon },
				{ "BULLET3", Mavi::Library::HasBullet3() && IsBuilderUsingPhysics() && !IsAddon },
				{ "RMLUI", Mavi::Library::HasRmlUI() && IsBuilderUsingGUI() && !IsAddon },
				{ "SHADERS", Mavi::Library::HasShaders() && IsBuilderUsingGraphics() && !IsAddon }
			};

			for (auto& Item : Features)
				ViFeatures += Form("set(VI_USE_%s %s CACHE BOOL \"-\")\n", Item.first.c_str(), Item.second ? "ON" : "OFF").R();

			if (!ViFeatures.empty())
				ViFeatures.erase(ViFeatures.end() - 1);
		}

		static String ViVersion;
		if (ViVersion.empty())
			ViVersion = GetViVersion();

		Stringify Target(&Data);
		Target.Replace("{{BUILDER_CONFIG_SETTINGS}}", ConfigSettingsArray);
		Target.Replace("{{BUILDER_CONFIG_LIBRARIES}}", ConfigLibrariesArray);
		Target.Replace("{{BUILDER_CONFIG_FUNCTIONS}}", ConfigFunctionsArray);
		Target.Replace("{{BUILDER_CONFIG_ADDONS}}", ConfigSystemAddonsArray);
		Target.Replace("{{BUILDER_CONFIG_SYSTEM_ADDONS}}", Config.Addons ? "true" : "false");
		Target.Replace("{{BUILDER_CONFIG_CLIBRARIES}}", Config.CLibraries ? "true" : "false");
		Target.Replace("{{BUILDER_CONFIG_CFUNCTIONS}}", Config.CFunctions ? "true" : "false");
		Target.Replace("{{BUILDER_CONFIG_FILES}}", Config.Files ? "true" : "false");
		Target.Replace("{{BUILDER_CONFIG_REMOTES}}", Config.Remotes ? "true" : "false");
		Target.Replace("{{BUILDER_CONFIG_TRANSLATOR}}", Config.Translator ? "true" : "false");
		Target.Replace("{{BUILDER_CONFIG_ESSENTIALS_ONLY}}", Config.EssentialsOnly ? "true" : "false");
		Target.Replace("{{BUILDER_MAVI_URL}}", REPOSITORY_TARGET_MAVI);
		Target.Replace("{{BUILDER_FEATURES}}", ViFeatures);
		Target.Replace("{{BUILDER_VERSION}}", ViVersion);
		Target.Replace("{{BUILDER_OUTPUT}}", Contextual.Name.empty() ? (IsAddon ? "addon_target" : "build_target") : Contextual.Name);
		return 0;
	}
	int BuilderAppendByteCode(const String& Path)
	{
		ByteCodeInfo Info;
		Info.Debug = Config.Debug;
		if (Unit->SaveByteCode(&Info) < 0)
		{
			VI_ERR("cannot fetch the byte code");
			return -1;
		}

		OS::Directory::Patch(OS::Path::GetDirectory(Path.c_str()));
		Stream* TargetFile = OS::File::Open(Path, FileMode::Binary_Write_Only);
		if (!TargetFile)
		{
			VI_ERR("cannot create the byte code file: %s", Path.c_str());
			return -1;
		}

		String Data = Codec::HexEncode((const char*)Info.Data.data(), Info.Data.size());
		if (TargetFile->Write(Data.data(), Data.size()) != Data.size())
		{
			VI_ERR("cannot write the byte code file: %s", Path.c_str());
			VI_RELEASE(TargetFile);
			return -1;
		}

		VI_RELEASE(TargetFile);
		return 0;
	}
	int BuilderAppendDependencies(const String& TargetDirectory)
	{
		bool IsVM = false;
		for (auto& Item : Contextual.Addons)
		{
			String From = GetBuilderAddonTargetLibrary(Item, &IsVM);
			if (IsVM)
				continue;

			String To = TargetDirectory + OS::Path::GetFilename(From.c_str());
			if (!OS::File::Copy(From.c_str(), To.c_str()))
			{
				VI_ERR("cannot copy dependant addon:\n\tfrom: %s\n\tto: %s", From.c_str(), To.c_str());
				return 1;
			}
		}
		
		return 0;
	}
	Schema* GetBuilderAddonInfo(const String& Name)
	{
		String LocalTarget = Contextual.Registry + Name + VI_SPLITTER + FILE_ADDON;
		return Schema::FromJSON(OS::File::ReadAsString(LocalTarget), false);
	}
	bool IsBuilderAddonCached(const String& Name)
	{
		String LocalTarget = GetBuilderAddonTarget(Name);
		if (OS::File::IsExists(LocalTarget.c_str()))
			return true;

		for (auto& Item : VM->GetCompileIncludeOptions().Exts)
		{
			String LocalTargetExt = LocalTarget + Item;
			if (OS::File::IsExists(LocalTargetExt.c_str()))
				return true;
		}

		return false;
	}
	bool IsBuilderAddonsCacheRegistryEmpty()
	{
		String GlobalTarget = Contextual.Registry + "mavi";
		return !OS::Directory::IsExists(GlobalTarget.c_str());
	}
	bool IsBuilderDirectoryEmpty(const String& Target)
	{
		Vector<FileEntry> Entries;
		return !OS::Directory::Scan(Target, &Entries) || Entries.empty();
	}
	bool IsBuilderUsingCompression()
	{
		return VM->HasSystemAddon("std/file_system") || IsBuilderUsingCrypto();
	}
	bool IsBuilderUsingCrypto()
	{
		return VM->HasSystemAddon("std/random") || VM->HasSystemAddon("std/crypto") || VM->HasSystemAddon("std/network") || VM->HasSystemAddon("std/engine");
	}
	bool IsBuilderUsingAudio()
	{
		return VM->HasSystemAddon("std/audio");
	}
	bool IsBuilderUsingGraphics()
	{
		return VM->HasSystemAddon("std/graphics");
	}
	bool IsBuilderUsingEngine()
	{
		return VM->HasSystemAddon("std/engine");
	}
	bool IsBuilderUsingPostgreSQL()
	{
		return VM->HasSystemAddon("std/postgresql");
	}
	bool IsBuilderUsingMongoDB()
	{
		return VM->HasSystemAddon("std/mongodb");
	}
	bool IsBuilderUsingPhysics()
	{
		return VM->HasSystemAddon("std/physics");
	}
	bool IsBuilderUsingGUI()
	{
		return VM->HasSystemAddon("std/gui/control") || VM->HasSystemAddon("std/gui/model") || VM->HasSystemAddon("std/gui/context");
	}
	String GetBuilderAddonTarget(const String& Name)
	{
		return Form("%s%s%cbin%c%s", Contextual.Registry.c_str(), Name.c_str(), VI_SPLITTER, VI_SPLITTER, OS::Path::GetFilename(Name.c_str())).R();
	}
	String GetBuilderAddonTargetLibrary(const String& Name, bool* IsVM)
	{
		if (IsVM)
			*IsVM = false;

		String Path1 = OS::Path::Resolve(GetBuilderAddonTarget(Name).c_str());
		if (OS::File::IsExists(Path1.c_str()))
			return Path1;

		for (auto& Ext : VM->GetCompileIncludeOptions().Exts)
		{
			String Path = Path1 + Ext;
			if (OS::File::IsExists(Path.c_str()))
				return Path;
		}

		UPtr<Schema> Info = GetBuilderAddonInfo(Name);
		if (Info->GetVar("type").GetBlob() != "vm")
			return Path1;

		if (IsVM)
			*IsVM = true;

		String Index = Info->GetVar("index").GetBlob();
		Path1 = Contextual.Registry + Name + VI_SPLITTER + Index;
		Path1 = OS::Path::Resolve(Path1.c_str());
		return Path1;
	}
	String GetBuilderDirectory(const String& LocalTarget)
	{
		return (Config.FastBuilds ? Contextual.Registry + "." : LocalTarget) + "make";
	}
	String GetViVersion()
	{
		return ToString((size_t)Mavi::MAJOR_VERSION) + '.' + ToString((size_t)Mavi::MINOR_VERSION) + '.' + ToString((size_t)Mavi::PATCH_VERSION);
	}
	const char* GetBuilderBuildType(bool IsAddon)
	{
#ifndef NDEBUG
		if (IsAddon)
			return "Debug";

		return (Config.Debug ? "Debug" : "RelWithDebInfo");
#else
		if (IsAddon)
			return "Release";

		return (Config.Debug ? "Debug" : "Release");
#endif
	}
	std::chrono::milliseconds GetTime()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
	}
};

int main(int argc, char* argv[])
{
	Mavias* Instance = new Mavias(argc, argv);
    Mavi::Initialize(Instance->WantsAllFeatures() ? (size_t)Mavi::Preset::Game : (size_t)Mavi::Preset::App);
	int ExitCode = Instance->Dispatch();
	delete Instance;
	Mavi::Uninitialize();
	
	return ExitCode;
}