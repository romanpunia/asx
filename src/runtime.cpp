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
	VirtualMachine* VM;
	Compiler* Unit;

public:
	Mavias(int ArgsCount, char** Args) : Contextual(ArgsCount, Args), VM(nullptr), Unit(nullptr)
	{
		AddDefaultCommands();
		AddDefaultSettings();
		ListenForSignals();
		Config.EssentialsOnly = !Contextual.Params.Has("graphics", "g");
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

		int Code = ConfigureEngine(Config, Contextual, VM);
		if (Code != 0)
			return Code;

		if (Config.Debug)
			VM->SetDebugger(new DebuggerContext());

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

		TypeInfo Type = VM->GetTypeInfoByDecl("array<string>@");
		Bindings::Array* ArgsArray = Bindings::Array::Compose<String>(Type.GetTypeInfo(), Contextual.Args);
		Context->Execute(Main, [&Main, ArgsArray](ImmediateContext* Context)
		{
			if (Main.GetArgsCount() > 0)
				Context->SetArgObject(0, ArgsArray);
		}).Wait();

		int ExitCode = Main.GetReturnTypeId() == (int)TypeId::VOIDF ? 0 : (int)Context->GetReturnDWord();
		VM->ReleaseObject(ArgsArray, Type);
		AwaitContext(Queue, VM, Context);
		return ExitCode;
	}
	void Shutdown()
	{
		static size_t Exits = 0;
		{
			auto* App = Application::Get();
			if (App != nullptr && App->GetState() == ApplicationState::Active)
			{
				App->Stop();
				goto GracefulShutdown;
			}

			auto* Queue = Schedule::Get();
			if (Queue->IsActive())
			{
				Queue->Stop();
				goto GracefulShutdown;
			}

			if (Exits > 0)
				VI_DEBUG("program is not responding; killed");

			return std::exit(0);
		}
	GracefulShutdown:
		++Exits;
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
			String Directory = OS::Directory::Get();
			size_t Index = 0; bool FileFlag = false;

			for (size_t i = 0; i < Contextual.Args.size(); i++)
			{
				auto& Value = Contextual.Args[i];
				if (!FileFlag)
				{
					FileFlag = (Value == "-f" || Value == "--file");
					continue;
				}

				auto File = OS::Path::Resolve(Value, Directory);
				if (OS::File::State(File, &Contextual.File))
				{
					Contextual.Path = File;
					Index = i;
					break;
				}

				File = OS::Path::Resolve(Value + (Config.LoadByteCode ? ".as.gz" : ".as"), Directory);
				if (OS::File::State(File, &Contextual.File))
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
		AddCommand("-i, --interactive", "enable interactive mode", [this](const String )
		{
			Config.Interactive = true;
			return JUMP_CODE + EXIT_CONTINUE;
		});
		AddCommand("-o, --output", "directory where to build an executable from source code", [this](const String& Path)
		{
			FileEntry File;
			Contextual.Output = (Path == "." ? OS::Directory::Get() : OS::Path::Resolve(Path, OS::Directory::Get()));
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
		AddCommand("-ia, --init", "initialize an addon template in given directory", [this](const String& Path)
		{
			FileEntry File;
			Contextual.Addon = (Path == "." ? OS::Directory::Get() : OS::Path::Resolve(Path, OS::Directory::Get()));
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
		AddCommand("--uses, --settings, --properties", "show virtual machine properties message", [this](const String&)
		{
			PrintProperties();
			return JUMP_CODE + EXIT_OK;
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
	int BuilderCreateExecutable()
	{
		if (IsBuilderDirectoryEmpty() && BuilderExecute("git clone --recursive https://github.com/romanpunia/template.as " + Contextual.Output) != 0)
		{
			VI_ERR("cannot download executable repository:\n\tmake sure you have git installed");
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
#ifndef NDEBUG
		String BuildType = (Config.Debug ? "Debug" : "RelWithDebInfo");
#else
		String BuildType = (Config.Debug ? "Debug" : "Release");
#endif
#ifdef VI_MICROSOFT
		String ConfigureCommand = Form("cmake -S %s -B %smake", Contextual.Output.c_str(), Contextual.Output.c_str()).R();
#else
		String ConfigureCommand = Form("cmake -DCMAKE_BUILD_TYPE=%s -S %s -B %smake", BuildType.c_str(), Contextual.Output.c_str(), Contextual.Output.c_str()).R();
#endif
		if (BuilderExecute(ConfigureCommand) != 0)
		{
#ifdef VI_MICROSOFT
			VI_ERR("cannot configure an executable repository:\n\tmake sure you vcpkg installed and VCPKG_ROOT env is set");
#else
			VI_ERR("cannot configure an executable repository:\n\tmake sure you have all dependencies installed");
#endif
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

#ifdef VI_MICROSOFT
		String BuildCommand = Form("cmake --build %smake --config %s", Contextual.Output.c_str(), BuildType.c_str()).R();
#else
		String BuildCommand = Form("cmake --build %smake", Contextual.Output.c_str()).R();
#endif
		if (BuilderExecute(BuildCommand) != 0)
		{
			VI_ERR("cannot build an executable repository:\n\tmake sure you have cmake installed");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		return JUMP_CODE + EXIT_OK;
	}
	int BuilderCreateAddon()
	{
		if (!IsBuilderDirectoryEmpty())
		{
			VI_ERR("cannot download addon repository:\n\ttarget directory is not empty: %s", Contextual.Addon.c_str());
			return JUMP_CODE + EXIT_ENTRYPOINT_FAILURE;
		}

		if (BuilderExecute("git clone --recursive https://github.com/romanpunia/addon.as " + Contextual.Addon) != 0)
		{
			VI_ERR("cannot download executable repository:\n\tmake sure you have git installed");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		if (BuilderGenerate(Contextual.Addon, true) != 0)
		{
			VI_ERR("cannot generate the template:\n\tmake sure application has file read/write permissions");
			return JUMP_CODE + EXIT_COMMAND_FAILURE;
		}

		return JUMP_CODE + EXIT_OK;
	}
	int BuilderExecute(const String& Command)
	{
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
			ViVersion = ToString((size_t)Mavi::MAJOR_VERSION) + '.' + ToString((size_t)Mavi::MINOR_VERSION) + '.' + ToString((size_t)Mavi::PATCH_VERSION);

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
	bool IsBuilderDirectoryEmpty()
	{
		Vector<FileEntry> Entries;
		return !OS::Directory::Scan(Contextual.Output, &Entries) || Entries.empty();
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