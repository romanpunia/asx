#include "app.h"
#include <signal.h>

namespace ASX
{
	Environment::Environment(int ArgsCount, char** Args) : Loop(nullptr), VM(nullptr), Context(nullptr), Unit(nullptr)
	{
		AddDefaultCommands();
		AddDefaultSettings();
		ListenForSignals();
		Env.Parse(ArgsCount, Args, Flags);
		ErrorHandling::SetFlag(LogOption::ReportSysErrors, false);
		ErrorHandling::SetFlag(LogOption::Active, true);
		ErrorHandling::SetFlag(LogOption::Pretty, false);
#ifndef NDEBUG
		OS::Directory::SetWorking(OS::Directory::GetModule()->c_str());
		Config.SaveSourceCode = true;
#endif
		Config.EssentialsOnly = !Env.Commandline.Has("game", "g") && !Env.Commandline.Has("interactive", "I");
		Config.Install = Env.Commandline.Has("install", "i") || Env.Commandline.Has("target");
	}
	Environment::~Environment()
	{
		Templates::Cleanup();
		if (Console::HasInstance())
			Console::Get()->Detach();
		Memory::Release(Context);
		Memory::Release(Unit);
		Memory::Release(VM);
		Memory::Release(Loop);
	}
	int Environment::Dispatch()
	{
		auto* Terminal = Console::Get();
		Terminal->Attach();

		VM = new VirtualMachine();
		for (auto& Next : Env.Commandline.Args)
		{
			if (Next.first == "__path__")
				continue;

			auto* Command = FindArgument(Next.first);
			if (!Command)
			{
				VI_ERR("command <%s> is not a valid operation", Next.first.c_str());
				return (int)ExitStatus::InvalidCommand;
			}

			int ExitCode = Command->Callback(Next.second);
			if (ExitCode != (int)ExitStatus::Continue)
				return ExitCode;
		}

		if (!Env.Commandline.Params.empty())
		{
			String Directory = *OS::Directory::GetWorking();
			auto File = OS::Path::Resolve(Env.Commandline.Params.front(), Directory, true);
			if (!File || !OS::File::GetState(*File, &Env.File) || Env.File.IsDirectory)
			{
				File = OS::Path::Resolve(Env.Commandline.Params.front() + (Config.LoadByteCode ? ".as.gz" : ".as"), Directory, true);
				if (File && OS::File::GetState(*File, &Env.File) && !Env.File.IsDirectory)
					Env.Path = *File;
			}
			else
				Env.Path = *File;

			if (!Env.File.IsExists)
			{
				VI_ERR("path <%s> does not exist", Env.Commandline.Params.front().c_str());
				return (int)ExitStatus::InputError;
			}

			Env.Registry = OS::Path::GetDirectory(Env.Path);
			if (Env.Registry == Env.Path)
			{
				Env.Path = Directory + Env.Path;
				Env.Registry = OS::Path::GetDirectory(Env.Path);
			}

			Env.Module = OS::Path::GetFilename(Env.Path).data();
			Env.Program = *OS::File::ReadAsString(Env.Path);
			Env.Registry += "addons";
			Env.Registry += VI_SPLITTER;
		}

		if (!Config.Interactive && Env.Addon.empty() && Env.Program.empty())
		{
			Config.Interactive = true;
			if (Env.Commandline.Args.size() > 1)
			{
				VI_ERR("provide a path to existing script file");
				return (int)ExitStatus::InputError;
			}
		}
		else if (!Env.Addon.empty())
		{
			if (Builder::InitializeIntoAddon(Config, Env, VM, Builder::GetDefaultSettings()) != StatusCode::OK)
				return (int)ExitStatus::CommandError;

			Terminal->WriteLine("Initialized " + Env.Mode + " addon: " + Env.Addon);
			return (int)ExitStatus::OK;
		}

		Unit = VM->CreateCompiler();
		if (!Runtime::ConfigureContext(Config, Env, VM, Unit))
			return (int)ExitStatus::CompilerError;

		OS::Directory::SetWorking(OS::Path::GetDirectory(Env.Path.c_str()).c_str());
		if (Config.Debug)
		{
			DebuggerContext* Debugger = new DebuggerContext();
			Debugger->SetInterruptCallback([](bool IsInterrupted)
			{
				if (!Schedule::IsAvailable())
					return;

				auto* Queue = Schedule::Get();
				if (IsInterrupted)
					Queue->Suspend();
				else
					Queue->Resume();
			});
			VM->SetDebugger(Debugger);
		}

		Unit->SetIncludeCallback(std::bind(&Environment::ImportAddon, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		auto Status = Unit->Prepare(Env.Module);
		if (!Status)
		{
			VI_ERR("cannot prepare <%s> module scope\n  %s", Env.Module, Status.Error().what());
			return (int)ExitStatus::PrepareError;
		}

		Context = VM->RequestContext();
		if (!Env.Program.empty())
		{
			if (!Config.LoadByteCode)
			{
				Status = Unit->LoadCode(Env.Path, Env.Program);
				if (!Status)
				{
					VI_ERR("cannot load <%s> module script code\n  %s", Env.Module, Status.Error().what());
					return (int)ExitStatus::LoadingError;
				}

				Runtime::ConfigureSystem(Config);
				Status = Unit->Compile().Get();
				if (!Status)
				{
					VI_ERR("cannot compile <%s> module\n  %s", Env.Module, Status.Error().what());
					return (int)ExitStatus::CompilerError;
				}
			}
			else
			{
				ByteCodeInfo Info;
				Info.Data.insert(Info.Data.begin(), Env.Program.begin(), Env.Program.end());

				Runtime::ConfigureSystem(Config);
				Status = Unit->LoadByteCode(&Info).Get();
				if (!Status)
				{
					VI_ERR("cannot load <%s> module bytecode\n  %s", Env.Module, Status.Error().what());
					return (int)ExitStatus::LoadingError;
				}
			}
		}

		if (Config.Install)
		{
			if (Config.Installed > 0)
			{
				Terminal->WriteLine("Successfully installed " + ToString(Config.Installed) + String(Config.Installed > 1 ? " addons" : " addon"));
				return EXIT_SUCCESS;
			}
			else if (Env.Output.empty())
				return Builder::PullAddonRepository(Config, Env) == StatusCode::OK ? (int)ExitStatus::OK : (int)ExitStatus::CommandError;

			if (Builder::CompileIntoExecutable(Config, Env, VM, Builder::GetDefaultSettings()) != StatusCode::OK)
				return (int)ExitStatus::CommandError;

			Terminal->WriteLine("Built binaries directory: " + Env.Output + "bin");
			return (int)ExitStatus::OK;
		}
		else if (Config.SaveByteCode)
		{
			ByteCodeInfo Info;
			Info.Debug = Config.Debug;
			if (Unit->SaveByteCode(&Info) && OS::File::Write(Env.Path + ".gz", (uint8_t*)Info.Data.data(), Info.Data.size()))
				return (int)ExitStatus::OK;

			VI_ERR("cannot save <%s> module bytecode", Env.Module);
			return (int)ExitStatus::SavingError;
		}
		else if (Config.Dependencies)
		{
			PrintDependencies();
			return (int)ExitStatus::OK;
		}
		else if (Config.Interactive)
		{
			if (Env.Path.empty())
				Env.Path = *OS::Directory::GetWorking();

			String Data, Multidata;
			Data.reserve(1024 * 1024);
			VM->ImportSystemAddon("*");
			PrintIntroduction("interactive mode");

			auto* Debugger = new DebuggerContext(DebugType::Detach);
			char DefaultCode[] = "void main(){}";
			bool Editor = false;
			size_t Section = 0;

			Env.Path += Env.Module;
			Debugger->SetEngine(VM);

			Function Main = Runtime::GetEntrypoint(Env, Entrypoint, Unit, true);
			if (!Main.IsValid())
			{
				Status = Unit->LoadCode(Env.Path + ":0", DefaultCode);
				if (!Status)
				{
					VI_ERR("cannot load default entrypoint for interactive mode\n  %s", Status.Error().what());
					Memory::Release(Debugger);
					return (int)ExitStatus::LoadingError;
				}

				Runtime::ConfigureSystem(Config);
				Status = Unit->Compile().Get();
				if (!Status)
				{
					VI_ERR("cannot compile default module for interactive mode\n  %s", Status.Error().what());
					Memory::Release(Debugger);
					return (int)ExitStatus::CompilerError;
				}
			}

			for (;;)
			{
				if (!Editor)
					Terminal->Write("> ");

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

				Stringify::Trim(Data);
				if (Editor)
				{
					if (!Data.empty() && Data.back() == '\x4')
					{
						Data.erase(Data.end() - 1);
						if (Env.Inline && !Data.empty() && Data.back() != ';')
							Data += ';';

						Multidata += Data;
						goto ExitEditor;
					}

					if (Env.Inline && !Data.empty() && Data.back() != ';')
						Data += ';';

					Multidata += Data;
					continue;
				}
				else if (Data == ".help")
				{
					Terminal->WriteLine("  .mode   - switch between registering and executing the code");
					Terminal->WriteLine("  .help   - show available commands");
					Terminal->WriteLine("  .editor - enter editor mode");
					Terminal->WriteLine("  .exit   - exit interactive mode");
					Terminal->WriteLine("  *       - anything else will be interpreted as script code");
					continue;
				}
				else if (Data == ".mode")
				{
					Env.Inline = !Env.Inline;
					if (Env.Inline)
						Terminal->WriteLine("  evaluation mode: you may now execute your code");
					else
						Terminal->WriteLine("  register mode: you may now register script interfaces");
					continue;
				}
				else if (Data == ".editor")
				{
					Terminal->WriteLine("  editor mode: you may write multiple lines of code (Ctrl+D to finish)\n");
					Editor = true;
					continue;
				}
				else if (Data == ".exit")
					break;

			Execute:
				if (Data.empty())
					continue;

				if (!Env.Inline)
				{
					String Index = ":" + ToString(++Section);
					if (!Unit->LoadCode(Env.Path + Index, Data) || Unit->Compile().Get())
						continue;
				}

				auto Inline = Unit->CompileFunction(Data, "any@").Get();
				if (!Inline)
					continue;

				Bindings::Any* Value = nullptr;
				auto Execution = Context->ExecuteCall(*Inline, nullptr).Get();
				if (Execution && *Execution == Execution::Finished)
				{
					String Indent = "  ";
					Value = Context->GetReturnObject<Bindings::Any>();
					Terminal->WriteLine(Indent + Debugger->ToString(Indent, 3, Value, VM->GetTypeInfoByName("any").GetTypeId()));
				}
				else
					Context->Abort();
				Context->Unprepare();
			}

			Memory::Release(Debugger);
			ExitProcess(ExitStatus::OK);
			return (int)ExitStatus::OK;
		}

		Function Main = Runtime::GetEntrypoint(Env, Entrypoint, Unit);
		if (!Main.IsValid())
			return (int)ExitStatus::EntrypointError;

		if (Config.Debug)
			PrintIntroduction("debugger");

		int ExitCode = 0;
		TypeInfo Type = VM->GetTypeInfoByDecl("array<string>@");
		Bindings::Array* ArgsArray = Type.IsValid() ? Bindings::Array::Compose<String>(Type.GetTypeInfo(), Env.Commandline.Params) : nullptr;
		VM->SetExceptionCallback([](ImmediateContext* Context)
		{
			if (!Context->WillExceptionBeCaught())
				ExitProcess(ExitStatus::RuntimeError);
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

		Runtime::AwaitContext(Loop, VM, Context);
		return ExitCode;
	}
	void Environment::Shutdown(int Value)
	{
		{
			if (Runtime::TryContextExit(Env, Value))
			{
				Loop->Wakeup();
				VI_DEBUG("graceful shutdown using [signal vcall]");
				goto GracefulShutdown;
			}

			auto* App = Application::Get();
			if (App != nullptr && App->GetState() == ApplicationState::Active)
			{
				App->Stop();
				Loop->Wakeup();
				VI_DEBUG("graceful shutdown using [application stop]");
				goto GracefulShutdown;
			}

			if (Schedule::IsAvailable())
			{
				Schedule::Get()->Stop();
				Loop->Wakeup();
				VI_DEBUG("graceful shutdown using [scheduler stop]");
				goto GracefulShutdown;
			}

			VI_DEBUG("forcing shutdown using [kill]");
			return ExitProcess(ExitStatus::Kill);
		}
	GracefulShutdown:
		ListenForSignals();
	}
	void Environment::Interrupt(int Value)
	{
		if (Config.Debug && VM->GetDebugger() && VM->GetDebugger()->Interrupt())
			ListenForSignals();
		else
			Shutdown(Value);
	}
	void Environment::Abort(const char* Signal)
	{
		VI_PANIC(false, "%s which is a critical runtime error", Signal);
	}
	size_t Environment::GetInitFlags()
	{
		if (Config.Install)
			return (size_t)Vitex::Preset::App & ~(size_t)Vitex::Init::Providers;

		if (Config.EssentialsOnly)
			return (size_t)Vitex::Preset::App;

		return (size_t)Vitex::Preset::Game;
	}
	void Environment::AddDefaultCommands()
	{
		AddCommand("application", "-h, --help", "show help message", true, [this](const std::string_view&)
		{
			PrintHelp();
			return (int)ExitStatus::OK;
		});
		AddCommand("application", "-v, --version", "show version message", true, [this](const std::string_view&)
		{
			PrintIntroduction("runtime");
			return (int)ExitStatus::OK;
		});
		AddCommand("application", "--plain", "show detailed log messages as is", true, [this](const std::string_view&)
		{
			Config.PrettyProgress = false;
			ErrorHandling::SetFlag(LogOption::Pretty, false);
			return (int)ExitStatus::Continue;
		});
		AddCommand("application", "--quiet", "disable logging", true, [](const std::string_view&)
		{
			ErrorHandling::SetFlag(LogOption::Active, false);
			return (int)ExitStatus::Continue;
		});
		AddCommand("application", "--timings", "append date for each logging message", true, [](const std::string_view&)
		{
			ErrorHandling::SetFlag(LogOption::Dated, true);
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-b, --bytecode", "load gz compressed compiled bytecode and execute it as normal", true, [this](const std::string_view&)
		{
			Config.LoadByteCode = true;
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-s, --save", "save gz compressed compiled bytecode to a file near script file", true, [this](const std::string_view&)
		{
			Config.SaveByteCode = true;
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-I, --interactive", "run only in interactive mode", true, [this](const std::string_view&)
		{
			Config.Interactive = true;
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-d, --debug", "enable debugger interface", true, [this](const std::string_view&)
		{
			Config.Debug = true;
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-g, --game", "enable game engine mode for graphics and audio support", true, [this](const std::string_view&)
		{
			Config.EssentialsOnly = false;
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-p, --preserve", "enable in memory source code preservation for better exception messages", true, [this](const std::string_view&)
		{
			Config.SaveSourceCode = true;
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-D, --deny", "deny permissions by name [expects: plus(+) separated list]", false, [this](const std::string_view& Value)
		{
			for (auto& Item : Stringify::Split(Value, '+'))
			{
				auto Option = OS::Control::ToOption(Item);
				if (!Option)
				{
					VI_ERR("os access control option not found: %s (options = %s)", Item.c_str(), OS::Control::ToOptions());
					return (int)ExitStatus::InputError;
				}

				Config.Permissions[*Option] = false;
			}
			return (int)ExitStatus::Continue;
		});
		AddCommand("execution", "-A, --allow", "allow permissions by name [expects: plus(+) separated list]", false, [this](const std::string_view& Value)
		{
			for (auto& Item : Stringify::Split(Value, '+'))
			{
				auto Option = OS::Control::ToOption(Item);
				if (!Option)
				{
					VI_ERR("os access control option not found: %s (options = %s)", Item.c_str(), OS::Control::ToOptions());
					return (int)ExitStatus::InputError;
				}

				Config.Permissions[*Option] = true;
			}
			return (int)ExitStatus::Continue;
		});
		AddCommand("building", "--target", "set a CMake name for output target [expects: name]", false, [this](const std::string_view& Name)
		{
			Env.Name = Name;
			return (int)ExitStatus::Continue;
		});
		AddCommand("building", "--output", "directory where to build an executable from source code [expects: path]", false, [this](const std::string_view& Path)
		{
			FileEntry File;
			if (Path != ".")
			{
				auto Target = OS::Path::Resolve(Path, *OS::Directory::GetWorking(), true);
				if (Target)
					Env.Output = *Target;
			}
			else
				Env.Output = *OS::Directory::GetWorking();

			if (!Env.Output.empty() && (Env.Output.back() == '/' || Env.Output.back() == '\\'))
				Env.Output.erase(Env.Output.end() - 1);

			if (ExecuteArgument({ "target" }) == ExitStatus::InvalidCommand || Env.Name.empty())
			{
				Env.Name = OS::Path::GetFilename(Env.Output.c_str());
				if (Env.Name.empty())
				{
					VI_ERR("init directory is set but name was not specified: use --target");
					return (int)ExitStatus::InputError;
				}
			}

			Env.Output += VI_SPLITTER + Env.Name + VI_SPLITTER;
			if (!OS::File::GetState(Env.Output, &File))
			{
				OS::Directory::Patch(Env.Output);
				return (int)ExitStatus::Continue;
			}

			if (File.IsDirectory)
				return (int)ExitStatus::Continue;

			VI_ERR("output path <%s> must be a directory", Path.data());
			return (int)ExitStatus::InputError;
		});
		AddCommand("building", "--import-std", "import standard addon(s) by name [expects: plus(+) separated list]", false, [this](const std::string_view& Value)
		{
			for (auto& Item : Stringify::Split(Value, '+'))
				Config.SystemAddons.push_back(Item);

			return (int)ExitStatus::Continue;
		});
		AddCommand("building", "--import-user", "import user addon(s) by path [expects: plus(+) separated list]", false, [this](const std::string_view& Value)
		{
			for (auto& Item : Stringify::Split(Value, '+'))
				Config.Libraries.emplace_back(std::make_pair(Item, true));

			return (int)ExitStatus::Continue;
		});
		AddCommand("building", "--import-lib", "import clibrary(ies) by path [expects: plus(+) separated list]", false, [this](const std::string_view& Value)
		{
			for (auto& Item : Stringify::Split(Value, '+'))
				Config.Libraries.emplace_back(std::make_pair(Item, false));

			return (int)ExitStatus::Continue;
		});
		AddCommand("building", "--import-func", "import clibrary function by declaration [expects: clib_name:cfunc_name=asfunc_decl]", false, [this](const std::string_view& Value)
		{
			size_t Offset1 = Value.find(':');
			if (Offset1 == std::string::npos)
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", Value.data());
				return (int)ExitStatus::InvalidDeclaration;
			}

			size_t Offset2 = Value.find('=', Offset1);
			if (Offset2 == std::string::npos)
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", Value.data());
				return (int)ExitStatus::InvalidDeclaration;
			}

			String CLibraryName = String(Value.substr(0, Offset1));
			Stringify::Trim(CLibraryName);

			String CFunctionName = String(Value.substr(Offset1 + 1, Offset2 - Offset1 - 1));
			Stringify::Trim(CLibraryName);

			String Declaration = String(Value.substr(Offset2 + 1));
			Stringify::Trim(CLibraryName);

			if (CLibraryName.empty() || CFunctionName.empty() || Declaration.empty())
			{
				VI_ERR("invalid clibrary cfunction declaration <%s>", Value.data());
				return (int)ExitStatus::InvalidDeclaration;
			}

			auto& Data = Config.Functions[CLibraryName];
			Data.first = CFunctionName;
			Data.second = Declaration;
			return (int)ExitStatus::Continue;
		});
		AddCommand("building", "--prop", "set virtual machine property [expects: prop_name:prop_value]", false, [this](const std::string_view& Value)
		{
			auto Args = Stringify::Split(Value, ':');
			if (Args.size() != 2)
			{
				VI_ERR("invalid property declaration <%s>", Value.data());
				return (int)ExitStatus::InputError;
			}

			auto It = Settings.find(Stringify::Trim(Args[0]));
			if (It == Settings.end())
			{
				VI_ERR("invalid property name <%s>", Args[0].c_str());
				return (int)ExitStatus::InputError;
			}

			String& Data = Args[1];
			Stringify::Trim(Data);
			Stringify::ToLower(Data);

			if (Data.empty())
			{
			InputFailure:
				VI_ERR("property value <%s>: %s", Args[0].c_str(), Args[1].empty() ? "?" : Args[1].c_str());
				return (int)ExitStatus::InputError;
			}
			else if (!Stringify::HasInteger(Data))
			{
				if (Args[1] == "on" || Args[1] == "true")
					VM->SetProperty((Features)It->second, 1);
				else if (Args[1] == "off" || Args[1] == "false")
					VM->SetProperty((Features)It->second, 1);
				else
					goto InputFailure;
			}

			VM->SetProperty((Features)It->second, (size_t)*FromString<uint64_t>(Data));
			return (int)ExitStatus::Continue;
		});
		AddCommand("building", "--props", "show virtual machine properties message", true, [this](const std::string_view&)
		{
			PrintProperties();
			return (int)ExitStatus::OK;
		});
		AddCommand("addons", "-a, --addon", "initialize an addon in given directory [expects: [native|vm]:?relpath]", false, [this](const std::string_view& Value)
		{
			String Path = String(Value);
			size_t Where = Value.find(':');
			if (Where != std::string::npos)
			{
				Path = Path.substr(Where + 1);
				if (Path.empty())
				{
					VI_ERR("addon initialization expects <mode:path> format: path must not be empty");
					return (int)ExitStatus::InputError;
				}

				Env.Mode = Value.substr(0, Where);
				if (Env.Mode != "native" && Env.Mode != "vm")
				{
					VI_ERR("addon initialization expects <mode:path> format: mode <%s> is invalid, [native|vm] expected", Env.Mode.c_str());
					return (int)ExitStatus::InputError;
				}
			}
			else
				Env.Mode = "vm";

			FileEntry File;
			if (Path != ".")
			{
				auto Target = OS::Path::Resolve(Path, *OS::Directory::GetWorking(), true);
				if (Target)
					Env.Addon = *Target;
			}
			else
				Env.Addon = *OS::Directory::GetWorking();

			if (!Env.Addon.empty() && (Env.Addon.back() == '/' || Env.Addon.back() == '\\'))
				Env.Addon.erase(Env.Addon.end() - 1);

			if (ExecuteArgument({ "target" }) == ExitStatus::InvalidCommand || Env.Name.empty())
			{
				Env.Name = OS::Path::GetFilename(Env.Addon.c_str());
				if (Env.Name.empty())
				{
					VI_ERR("init directory is set but name was not specified: use --target");
					return (int)ExitStatus::InputError;
				}
			}

			Env.Addon += VI_SPLITTER + Env.Name + VI_SPLITTER;
			if (!OS::File::GetState(Env.Addon, &File))
			{
				OS::Directory::Patch(Env.Addon);
				return (int)ExitStatus::Continue;
			}

			if (File.IsDirectory)
				return (int)ExitStatus::Continue;

			VI_ERR("addon path <%s> must be a directory", Path.c_str());
			return (int)ExitStatus::InputError;
		});
		AddCommand("addons", "-i, --install", "install or update script dependencies", true, [this](const std::string_view& Value)
		{
			Config.Install = true;
			return (int)ExitStatus::Continue;
		});
		AddCommand("addons", "--deps", "install and show dependencies message", true, [this](const std::string_view&)
		{
			Config.Dependencies = true;
			return (int)ExitStatus::Continue;
		});
	}
	void Environment::AddDefaultSettings()
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
	void Environment::AddCommand(const std::string_view& Category, const std::string_view& Name, const std::string_view& Description, bool IsFlagOnly, const CommandCallback& Callback)
	{
		EnvironmentCommand Command;
		Command.Arguments = Stringify::Split(Name, ',');
		Command.Description = Description;
		Command.Callback = Callback;

		for (auto& Argument : Command.Arguments)
		{
			Stringify::Trim(Argument);
			while (!Argument.empty() && Argument.front() == '-')
				Argument.erase(Argument.begin());
			if (IsFlagOnly)
				Flags.insert(Argument);
		}

		auto& Target = Commands[String(Category)];
		Target.push_back(std::move(Command));
	}
	ExitStatus Environment::ExecuteArgument(const UnorderedSet<String>& Names)
	{
		for (auto& Next : Env.Commandline.Args)
		{
			if (Names.find(Next.first) == Names.end())
				continue;

			auto* Command = FindArgument(Next.first);
			if (!Command)
				return ExitStatus::InvalidCommand;

			int ExitCode = Command->Callback(Next.second);
			if (ExitCode != (int)ExitStatus::Continue)
				return (ExitStatus)ExitCode;
		}

		return ExitStatus::OK;
	}
	EnvironmentCommand* Environment::FindArgument(const std::string_view& Name)
	{
		for (auto& Category : Commands)
		{
			for (auto& Command : Category.second)
			{
				for (auto& Argument : Command.Arguments)
				{
					if (Argument == Name)
						return &Command;
				}
			}
		}

		return nullptr;
	}
	ExpectsPreprocessor<IncludeType> Environment::ImportAddon(Preprocessor* Base, const IncludeResult& File, String& Output)
	{
		if (File.Module.empty() || File.Module.front() != '@')
			return IncludeType::Unchanged;

		if (!Control::Has(Config, AccessOption::Https))
		{
			VI_ERR("cannot import addon <%s> from remote repository: permission denied", File.Module.c_str());
			return IncludeType::Error;
		}

		IncludeType Status;
		if (!Builder::IsAddonTargetExists(Env, VM, File.Module))
		{
			if (!Config.Install)
			{
				VI_ERR("program requires <%s> addon: run installation with --install flag", File.Module.c_str());
				Status = IncludeType::Error;
			}
			else if (Builder::CompileIntoAddon(Config, Env, VM, File.Module, Output) == StatusCode::OK)
			{
				Status = Output.empty() ? IncludeType::Virtual : IncludeType::Preprocess;
				++Config.Installed;
			}
			else
				Status = IncludeType::Error;
		}
		else if (Builder::ImportIntoAddon(Env, VM, File.Module, Output) == StatusCode::OK)
			Status = Output.empty() ? IncludeType::Virtual : IncludeType::Preprocess;
		else
			Status = IncludeType::Error;
		Env.Addons.insert(File.Module);
		return Status;
	}
	void Environment::PrintIntroduction(const char* Label)
	{
		auto* Terminal = Console::Get();
		auto* Lib = Vitex::Runtime::Get();
		Terminal->Write("Welcome to ASX ");
		Terminal->Write(Label);
		Terminal->Write(" v");
		Terminal->Write(ToString((uint32_t)Vitex::MAJOR_VERSION));
		Terminal->Write(".");
		Terminal->Write(ToString((uint32_t)Vitex::MINOR_VERSION));
		Terminal->Write(".");
		Terminal->Write(ToString((uint32_t)Vitex::PATCH_VERSION));
		Terminal->Write(" [");
		Terminal->Write(Lib->GetCompiler());
		Terminal->Write(" ");
		Terminal->Write(Lib->GetBuild());
		Terminal->Write(" on ");
		Terminal->Write(Lib->GetPlatform());
		Terminal->Write("]\n");
		Terminal->Write("Run \"" + String(Config.Interactive ? ".help" : (Config.Debug ? "help" : "asx --help")) + "\" for more information");
		if (Config.Interactive)
			Terminal->Write(" (loaded " + ToString(VM->GetExposedAddons().size()) + " addons)");
		Terminal->Write("\n");
	}
	void Environment::PrintHelp()
	{
		size_t Max = 0;
		for (auto& Category : Commands)
		{
			for (auto& Next : Category.second)
			{
				size_t Size = 0;
				for (auto& Argument : Next.Arguments)
					Size += (Argument.size() > 1 ? 2 : 1) + Argument.size() + 2;
				Size -= 2;
				if (Size > Max)
					Max = Size;
			}
		}

		auto* Terminal = Console::Get();
		Terminal->WriteLine("Usage: asx [options?...]");
		Terminal->WriteLine("       asx [options?...] [file.as] [arguments?...]\n");
		for (auto& Category : Commands)
		{
			String Name = Category.first;
			Terminal->WriteLine("Category: " + Stringify::ToUpper(Name));
			for (auto& Next : Category.second)
			{
				String Command;
				for (auto& Argument : Next.Arguments)
					Command += (Argument.size() > 1 ? "--" : "-") + Argument + ", ";

				Command.erase(Command.size() - 2, 2);
				size_t Spaces = Max - Command.size();
				Terminal->Write("    ");
				Terminal->Write(Command);
				for (size_t i = 0; i < Spaces; i++)
					Terminal->Write(" ");
				Terminal->WriteLine(" - " + Next.Description);
			}
			Terminal->WriteChar('\n');
		}
	}
	void Environment::PrintProperties()
	{
		auto* Terminal = Console::Get();
		for (auto& Item : Settings)
		{
			size_t Value = VM->GetProperty((Features)Item.second);
			Terminal->Write("  " + Item.first + ": ");
			if (Stringify::EndsWith(Item.first, "mode"))
				Terminal->Write("mode " + ToString(Value));
			else if (Stringify::EndsWith(Item.first, "size"))
				Terminal->Write((Value > 0 ? ToString(Value) : "unlimited"));
			else if (Value == 0)
				Terminal->Write("OFF");
			else if (Value == 1)
				Terminal->Write("ON");
			else
				Terminal->Write(ToString(Value));
			Terminal->Write("\n");
		}
	}
	void Environment::PrintDependencies()
	{
		auto* Terminal = Console::Get();
		auto Exposes = VM->GetExposedAddons();
		if (!Exposes.empty())
		{
			Terminal->WriteLine("  local dependencies list:");
			for (auto& Item : Exposes)
				Terminal->WriteLine("    " + Stringify::Replace(Item, ":", ": "));
		}

		if (!Env.Addons.empty())
		{
			Terminal->WriteLine("  remote dependencies list:");
			for (auto& Item : Env.Addons)
				Terminal->WriteLine("    " + Item + ": " + Builder::GetAddonTargetLibrary(Env, VM, Item, nullptr));
		}
	}
	void Environment::ListenForSignals()
	{
		static Environment* Instance = this;
		signal(SIGINT, [](int Value) { Instance->Interrupt(Value); });
		signal(SIGTERM, [](int Value) { Instance->Shutdown(Value); });
		signal(SIGFPE, [](int) { Instance->Abort("division by zero"); });
		signal(SIGILL, [](int) { Instance->Abort("illegal instruction"); });
		signal(SIGSEGV, [](int) { Instance->Abort("segmentation fault"); });
#ifdef VI_UNIX
		signal(SIGPIPE, SIG_IGN);
		signal(SIGCHLD, SIG_IGN);
#endif
	}
	void Environment::ExitProcess(ExitStatus Code)
	{
		if (Code != ExitStatus::RuntimeError)
			OS::Process::Exit((int)Code);
		else
			OS::Process::Abort();
	}
}

int main(int argc, char* argv[])
{
	auto* Instance = new ASX::Environment(argc, argv);
	Vitex::Runtime Scope(Instance->GetInitFlags());
	int ExitCode = Instance->Dispatch();
	delete Instance;
	return ExitCode;
}