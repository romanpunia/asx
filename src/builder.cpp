#include "builder.h"
#include "code.hpp"
#include <iostream>
#define REPOSITORY_SOURCE "https://github.com/"
#define REPOSITORY_TARGET_VITEX "https://github.com/romanpunia/vitex"
#define REPOSITORY_FILE_INDEX "addon.as"
#define REPOSITORY_FILE_ADDON "addon.json"

namespace ASX
{
	StatusCode Builder::CompileIntoAddon(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, String& Output)
	{
		String LocalTarget = Env.Registry + Name, RemoteTarget = Name.substr(1);
		if (IsDirectoryEmpty(LocalTarget) && ExecuteGit("git clone " REPOSITORY_SOURCE + RemoteTarget + " " + LocalTarget) != StatusCode::OK)
		{
			VI_ERR("addon <%s> does not seem to be available at remote repository: <%s>", RemoteTarget.c_str());
			return StatusCode::CommandError;
		}

		UPtr<Schema> Info = GetAddonInfo(Env, Name);
		if (!Info)
		{
			VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file", Name.c_str());
			return StatusCode::ConfigurationError;
		}

		String Type = Info->GetVar("type").GetBlob();
		if (Type == "native")
		{
			String VitexDirectory = GetGlobalVitexPath();
			if (!AppendVitex(String()))
			{
				VI_ERR("addon <%s> cannot be created: global target cannot be built", Name.c_str());
				return StatusCode::ConfigurationError;
			}

			String BuildDirectory = GetBuildingDirectory(Env, LocalTarget);
			OS::Directory::Remove(BuildDirectory.c_str());
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
			String ConfigureCommand = Stringify::Text("cmake -S %s -B %s -DVI_DIRECTORY=%s -DVI_CXX=%i", LocalTarget.c_str(), BuildDirectory.c_str(), VitexDirectory.c_str(), VI_CXX);
#else
			String ConfigureCommand = Stringify::Text("cmake -S %s -B %s -DVI_DIRECTORY=%s -DVI_CXX=%i -DCMAKE_BUILD_TYPE=%s", LocalTarget.c_str(), BuildDirectory.c_str(), VitexDirectory.c_str(), VI_CXX, GetBuildType(Config));
#endif
			if (ExecuteCMake(ConfigureCommand) != StatusCode::OK)
			{
				VI_ERR("addon <%s> cannot be created: final target cannot be configured", Name.c_str());
				return StatusCode::ConfigurationError;
			}
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
			String BuildCommand = Stringify::Text("cmake --build %s --config %s", BuildDirectory.c_str(), GetBuildType(Config));
#else
			String BuildCommand = Stringify::Text("cmake --build %s", BuildDirectory.c_str());
#endif
			if (ExecuteCMake(BuildCommand) != StatusCode::OK)
			{
				VI_ERR("addon <%s> cannot be created: final target cannot be built", Name.c_str());
				return StatusCode::BuildError;
			}

			String AddonName = OS::Path::GetFilename(Name.c_str());
			String TargetPath = GetAddonTarget(Env, Name);
			String NextPath = GetGlobalTargetsDirectory(Env, Name) + VI_SPLITTER;
			OS::Directory::Patch(NextPath);

			Vector<std::pair<String, FileEntry>> Files;
			String PrevPath = GetLocalTargetsDirectory(Env, Name) + VI_SPLITTER;
			OS::Directory::Scan(PrevPath, &Files);
			for (auto& File : Files)
			{
				String NextFilePath = NextPath + File.first;
				String PrevFilePath = PrevPath + File.first;
				if (!File.second.IsDirectory && Stringify::StartsWith(File.first, AddonName))
					OS::File::Move(PrevFilePath.c_str(), NextFilePath.c_str());
			}

			OS::Directory::Remove(PrevPath.c_str());
			return VM->ImportAddon(TargetPath) ? StatusCode::OK : StatusCode::DependencyError;
		}
		else if (Type == "vm")
		{
			String Index(Info->GetVar("index").GetBlob());
			if (Index.empty() || !Stringify::EndsWith(Index, ".as") || Stringify::FindOf(Index, "/\\").Found)
			{
				VI_ERR("addon <%s> cannot be created: index file <%s> is not valid", Name.c_str(), Index.c_str());
				return StatusCode::ConfigurationError;
			}

			String Path = LocalTarget + VI_SPLITTER + Index;
			if (!OS::File::IsExists(Path.c_str()))
			{
				VI_ERR("addon <%s> cannot be created: index file cannot be found", Name.c_str());
				return StatusCode::ConfigurationError;
			}

			Output = *OS::File::ReadAsString(Path.c_str());
			return StatusCode::OK;
		}

		VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file: type <%s> is not recognized", Name.c_str(), Type.c_str());
		return StatusCode::ConfigurationError;
	}
	StatusCode Builder::ImportIntoAddon(EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, String& Output)
	{
		UPtr<Schema> Info = GetAddonInfo(Env, Name);
		if (!Info)
		{
			VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file", Name.c_str());
			return StatusCode::ConfigurationError;
		}

		String Type = Info->GetVar("type").GetBlob();
		if (Type == "native")
		{
			String Path = GetAddonTarget(Env, Name);
			return VM->ImportAddon(Path) ? StatusCode::OK : StatusCode::DependencyError;
		}
		else if (Type == "vm")
		{
			String Path = Env.Registry + Name + VI_SPLITTER + Info->GetVar("index").GetBlob();
			if (!OS::File::IsExists(Path.c_str()))
			{
				VI_ERR("addon <%s> cannot be imported: index file cannot be found", Name.c_str());
				return StatusCode::ConfigurationError;
			}

			Output = *OS::File::ReadAsString(Path.c_str());
			return StatusCode::OK;
		}

		VI_ERR("addon <%s> does not seem to have a valid " REPOSITORY_FILE_ADDON " file: type <%s> is not recognized", Name.c_str(), Type.c_str());
		return StatusCode::ConfigurationError;
	}
	StatusCode Builder::InitializeIntoAddon(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings)
	{
		if (!IsDirectoryEmpty(Env.Addon))
		{
			VI_ERR("cannot clone addon repository: target directory is not empty: %s", Env.Addon.c_str());
			return StatusCode::ConfigurationError;
		}

		UnorderedMap<String, String> Keys = GetBuildKeys(Config, Env, VM, Settings, true);
		if (Env.Mode == "vm")
		{
			Vector<String> Files =
			{
				"addon/addon.json",
				"addon/addon.as"
			};

			Keys["BUILDER_INDEX"] = "\"" REPOSITORY_FILE_INDEX "\"";
			for (auto& File : Files)
			{
				if (!AppendTemplate(Keys, Env.Addon, File))
					return StatusCode::GenerationError;
			}

			return StatusCode::OK;
		}
		else if (Env.Mode == "native")
		{
			if (!AppendVitex(Env.Addon + "deps/vitex"))
			{
				VI_ERR("cannot clone executable repository");
				return StatusCode::CommandError;
			}

			UnorderedMap<String, String> Files =
			{
				{ "addon/CMakeLists.txt", "" },
				{ "addon/addon.json", "" },
				{ "addon/addon.cpp", "" },
				{ "", "make" }
			};

			Keys["BUILDER_INDEX"] = "null";
			for (auto& File : Files)
			{
				String TargetPath = Env.Addon + File.second;
				if (File.first.empty())
				{
					if (!OS::Directory::Create(TargetPath.c_str()))
					{
						VI_ERR("cannot generate the template in path: %s", TargetPath.c_str());
						return StatusCode::GenerationError;
					}
				}
				else if (!AppendTemplate(Keys, TargetPath, File.first))
					return StatusCode::GenerationError;
			}

			return StatusCode::OK;
		}

		return StatusCode::ConfigurationError;
	}
	StatusCode Builder::PullAddonRepository(EnvironmentConfig& Env)
	{
		if (Env.Registry.empty())
		{
			VI_ERR("provide entrypoint file to pull addons");
			return StatusCode::ConfigurationError;
		}

		Vector<std::pair<String, FileEntry>> Entries;
		if (!OS::Directory::Scan(Env.Registry.c_str(), &Entries) || Entries.empty())
			return StatusCode::OK;

		auto Pull = [](const String& Path) { return ExecuteGit("cd \"" + Path + "\" && git pull") == StatusCode::OK; };
		for (auto& File : Entries)
		{
			if (!File.second.IsDirectory || File.first.empty() || File.first.front() == '.')
				continue;

			if (File.first.front() == '@')
			{
				Vector<std::pair<String, FileEntry>> Addons;
				String RepositoriesPath = Env.Registry + File.first + VI_SPLITTER;
				if (!OS::Directory::Scan(RepositoriesPath.c_str(), &Addons) || Addons.empty())
					continue;

				for (auto& Addon : Addons)
				{
					if (Addon.second.IsDirectory && !Pull(RepositoriesPath + Addon.first))
					{
						VI_ERR("cannot pull addon target repository: %s", File.first.c_str());
						return StatusCode::CommandError;
					}
				}
			}
			else if (!Pull(Env.Registry + File.first))
			{
				VI_ERR("cannot pull addon source repository: %s", File.first.c_str());
				return StatusCode::CommandError;
			}
		}

		return StatusCode::OK;
	}
	StatusCode Builder::CompileIntoExecutable(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings)
	{
		String VitexDirectory = GetGlobalVitexPath();
		if (!AppendVitex(String()))
		{
			VI_ERR("cannot clone executable repository");
			return StatusCode::CommandError;
		}

		UnorderedMap<String, String> Keys = GetBuildKeys(Config, Env, VM, Settings, false);
		UnorderedMap<String, String> Files =
		{
			{ "executable/CMakeLists.txt", "" },
			{ "executable/vcpkg.json", "" },
			{ "executable/runtime.hpp", "" },
			{ "executable/program.cpp", "" },
			{ "", "make" }
		};

		for (auto& File : Files)
		{
			String TargetPath = Env.Output + File.second;
			if (File.first.empty())
			{
				if (!OS::Directory::Create(TargetPath.c_str()))
				{
					VI_ERR("cannot generate the template in path: %s", TargetPath.c_str());
					return StatusCode::GenerationError;
				}
			}
			else if (!AppendTemplate(Keys, TargetPath, File.first))
				return StatusCode::GenerationError;
		}

		if (!AppendByteCode(Config, Env, Env.Output + "program.b64"))
		{
			VI_ERR("cannot embed the byte code: make sure application has file read/write permissions");
			return StatusCode::ByteCodeError;
		}

		if (!AppendDependencies(Env, VM, Env.Output + "bin/"))
		{
			VI_ERR("cannot embed the dependencies: make sure application has file read/write permissions");
			return StatusCode::ConfigurationError;
		}
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		String ConfigureCommand = Stringify::Text("cmake -S %s -B %smake -DVI_DIRECTORY=%s -DVI_CXX=%i", Env.Output.c_str(), Env.Output.c_str(), VitexDirectory.c_str(), VI_CXX);
#else
		String ConfigureCommand = Stringify::Text("cmake -S %s -B %smake -DVI_DIRECTORY=%s -DVI_CXX=%i -DCMAKE_BUILD_TYPE=%s", Env.Output.c_str(), Env.Output.c_str(), VitexDirectory.c_str(), VI_CXX, GetBuildType(Config));
#endif
		if (ExecuteCMake(ConfigureCommand) != StatusCode::OK)
		{
#ifdef VI_MICROSOFT
			VI_ERR("cannot configure an executable repository: make sure you vcpkg installed and VCPKG_ROOT env is set");
#else
			VI_ERR("cannot configure an executable repository: make sure you have all dependencies installed");
#endif
			return StatusCode::ConfigurationError;
		}
#if defined(VI_MICROSOFT) || defined(VI_APPLE)
		String BuildCommand = Stringify::Text("cmake --build %smake --config %s", Env.Output.c_str(), GetBuildType(Config));
#else
		String BuildCommand = Stringify::Text("cmake --build %smake", Env.Output.c_str());
#endif
		if (ExecuteCMake(BuildCommand) != StatusCode::OK)
		{
			VI_ERR("cannot build an executable repository");
			return StatusCode::BuildError;
		}

		return StatusCode::OK;
	}
	UnorderedMap<String, uint32_t> Builder::GetDefaultSettings()
	{
		UnorderedMap<String, uint32_t> Settings;
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
		return Settings;
	}
	String Builder::GetSystemVersion()
	{
		return ToString((size_t)Vitex::MAJOR_VERSION) + '.' + ToString((size_t)Vitex::MINOR_VERSION) + '.' + ToString((size_t)Vitex::PATCH_VERSION) + '.' + ToString((size_t)Vitex::BUILD_VERSION);
	}
	StatusCode Builder::ExecuteGit(const String& Command)
	{
		static int IsGitInstalled = -1;
		if (IsGitInstalled == -1)
		{
			IsGitInstalled = ExecuteCommand("FIND", "git", 0x1) ? 1 : 0;
			if (!IsGitInstalled)
			{
				VI_ERR("cannot find <git> program, please make sure it is installed");
				return StatusCode::CommandNotFound;
			}
		}

		return ExecuteCommand("RUN", Command, 0x0) ? StatusCode::OK : StatusCode::CommandError;
	}
	StatusCode Builder::ExecuteCMake(const String& Command)
	{
		static int IsCMakeInstalled = -1;
		if (IsCMakeInstalled == -1)
		{
			IsCMakeInstalled = ExecuteCommand("FIND", "cmake", 0x0) ? 1 : 0;
			if (!IsCMakeInstalled)
			{
				VI_ERR("cannot find <cmake> program, please make sure it is installed");
				return StatusCode::CommandNotFound;
			}
		}

		return ExecuteCommand("RUN", Command, 0x0) ? StatusCode::OK : StatusCode::CommandError;
	}
	bool Builder::ExecuteCommand(const String& Label, const String& Command, int SuccessExitCode)
	{
		const uint32_t WindowSize = 10;
		uint32_t Height = 0, Y = 0;
		auto* Terminal = Console::Get();
		if (!Terminal->ReadScreen(nullptr, &Height, nullptr, &Y))
			Height = WindowSize;
		Height--;

		uint32_t Lines = std::min<uint32_t>(Y >= Height ? 0 : Y - Height, WindowSize);
		bool Logging = Lines > 0 && Label != "FIND", Loading = true;
		auto Time = Schedule::GetClock();
		uint64_t Title = Terminal->CaptureElement();
		uint64_t Window = Logging ? Terminal->CaptureWindow(Lines) : 0;
		std::thread Loader([Terminal, Title, &Time, &Loading, &Label, &Command]()
		{
			while (Loading)
			{
				auto Diff = (Schedule::GetClock() - Time).count() / 1000000.0;
				Terminal->SpinningElement(Title, "> " + Label + " " + Command + " - " + ToString(Diff) + " seconds");
				std::this_thread::sleep_for(std::chrono::milliseconds(60));
			}
		});

		SingleQueue<String> Messages;
		auto ExitCode = OS::Process::Execute(Command, FileMode::Read_Only, [Terminal, Window, Logging, &Messages](const char* Buffer, size_t Size)
		{
			size_t Index = Messages.size() + 1;
			String Text = (Index < 100 ? (Index < 10 ? "[00" : "[0") : "[") + ToString(Index) + "]  " + String(Buffer, Size);
			if (Logging)
			{
				Terminal->EmplaceWindow(Window, Text);
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			Messages.push(std::move(Text));
			return true;
		});

		bool Success = ExitCode && *ExitCode == SuccessExitCode;
		auto Diff = (Schedule::GetClock() - Time).count() / 1000000.0;
		Loading = false;
		Loader.join();

		Terminal->ReplaceElement(Title, "> " + Label + " " + Command + " - " + ToString(Diff) + " seconds: " + (Success ? String("OK") : (ExitCode ? "EXIT " + ToString(*ExitCode) : String("FAIL"))));
		Terminal->FreeElement(Title);
		if (Logging)
			Terminal->FreeWindow(Window, true);
		if (!ExitCode)
			Messages.push(ExitCode.What() + "\n");

		while (!Success && !Messages.empty())
		{
			Terminal->Write(Messages.front());
			Messages.pop();
		}

		return Success;
	}
	bool Builder::AppendTemplate(const UnorderedMap<String, String>& Keys, const String& TargetPath, const String& TemplatePath)
	{
		auto File = Templates::Fetch(Keys, TemplatePath);
		if (!File)
		{
			VI_ERR("cannot find the template: %s", TemplatePath.c_str());
			return false;
		}

		if (!OS::Directory::Patch(TargetPath))
		{
			VI_ERR("cannot generate the template in path: %s", TargetPath.c_str());
			return false;
		}

		String Path = TargetPath;
		String Filename = OS::Path::GetFilename(TemplatePath.c_str());
		if (Path.back() != '/' && Path.back() != '\\')
			Path += VI_SPLITTER;

		if (!OS::File::Write(Path + Filename, *File))
		{
			VI_ERR("cannot generate the template in path: %s - save failed", TargetPath.c_str());
			return false;
		}

		return true;
	}
	bool Builder::AppendByteCode(SystemConfig& Config, EnvironmentConfig& Env, const String& Path)
	{
		ByteCodeInfo Info;
		Info.Debug = Config.Debug;
		if (!Env.ThisCompiler->SaveByteCode(&Info))
		{
			VI_ERR("cannot fetch the byte code");
			return false;
		}

		OS::Directory::Patch(OS::Path::GetDirectory(Path.c_str()));
		auto TargetFile = OS::File::Open(Path, FileMode::Binary_Write_Only);
		if (!TargetFile)
		{
			VI_ERR("cannot create the byte code file: %s", Path.c_str());
			return false;
		}

		String Data = Codec::Base64Encode(Info.Data.data(), Info.Data.size());
		if (TargetFile->Write(Data.data(), Data.size()).Or(0) != Data.size())
		{
			VI_ERR("cannot write the byte code file: %s", Path.c_str());
			VI_RELEASE(TargetFile);
			return false;
		}

		VI_RELEASE(TargetFile);
		return true;
	}
	bool Builder::AppendDependencies(EnvironmentConfig& Env, VirtualMachine* VM, const String& TargetDirectory)
	{
		bool IsVM = false;
		for (auto& Item : Env.Addons)
		{
			String From = GetAddonTargetLibrary(Env, VM, Item, &IsVM);
			if (IsVM)
				continue;

			String To = TargetDirectory + OS::Path::GetFilename(From.c_str());
			if (!OS::File::Copy(From.c_str(), To.c_str()))
			{
				VI_ERR("cannot copy dependant addon: from: %s to: %s", From.c_str(), To.c_str());
				return false;
			}
		}

		return true;
	}
	bool Builder::AppendVitex(const String& TargetPath)
	{
		String SourcePath = GetGlobalVitexPath();
		if (IsDirectoryEmpty(SourcePath))
		{
			OS::Directory::Patch(SourcePath);
			if (ExecuteGit("git clone --recursive " REPOSITORY_TARGET_VITEX " " + SourcePath) != StatusCode::OK)
				return false;
		}
		
		if (TargetPath.empty() || TargetPath == SourcePath)
			return true;
#ifdef VI_MICROSOFT
		String CopyCommand = Stringify::Text("xcopy \"%s\" \"%s%s\" /s /h /e /k /f /c > nul", SourcePath.c_str(), TargetPath.c_str(), TargetPath.back() == '/' || TargetPath.back() == '\\' ? "" : "/");
#else
		String CopyCommand = Stringify::Text("cp -a \"%s%s.\" \"%s%s\"", SourcePath.c_str(), SourcePath.back() == '/' ? "" : "/", TargetPath.c_str(), TargetPath.back() == '/' ? "" : "/");
#endif
		return ExecuteCommand("RUN", CopyCommand, 0x0);
	}
	bool Builder::IsAddonTargetExists(EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, bool Nested)
	{
		String LocalTarget = (Nested ? Name : GetAddonTarget(Env, Name));
		if (OS::File::IsExists(LocalTarget.c_str()))
			return true;

		for (auto& Item : VM->GetCompileIncludeOptions().Exts)
		{
			String LocalTargetExt = LocalTarget + Item;
			if (OS::File::IsExists(LocalTargetExt.c_str()))
				return true;
		}

		if (Nested)
			return false;

		UPtr<Schema> Info = GetAddonInfo(Env, Name);
		if (Info && Info->GetVar("type").GetBlob() == "vm")
			return IsAddonTargetExists(Env, VM, Env.Registry + Name + VI_SPLITTER + Info->GetVar("index").GetBlob(), true);

		return false;
	}
	bool Builder::IsDirectoryEmpty(const String& Target)
	{
		Vector<std::pair<String, FileEntry>> Entries;
		return !OS::Directory::Scan(Target, &Entries) || Entries.empty();
	}
	bool Builder::IsUsingCompression(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("fs") || IsUsingCrypto(VM);
	}
	bool Builder::IsUsingSchemas(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("schema");
	}
	bool Builder::IsUsingCrypto(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("random") || VM->HasSystemAddon("crypto") || VM->HasSystemAddon("network") || VM->HasSystemAddon("engine");
	}
	bool Builder::IsUsingAudio(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("audio");
	}
	bool Builder::IsUsingGraphics(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("graphics");
	}
	bool Builder::IsUsingEngine(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("engine");
	}
	bool Builder::IsUsingSQLite(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("sqlite");
	}
	bool Builder::IsUsingPostgreSQL(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("postgresql");
	}
	bool Builder::IsUsingMongoDB(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("mongodb");
	}
	bool Builder::IsUsingPhysics(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("physics");
	}
	bool Builder::IsUsingGUI(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("gui-control") || VM->HasSystemAddon("gui-model") || VM->HasSystemAddon("gui-context");
	}
	bool Builder::IsUsingOS(VirtualMachine* VM)
	{
		return VM->HasSystemAddon("os");
	}
	const char* Builder::GetBuildType(SystemConfig& Config)
	{
#ifndef NDEBUG
		return "Debug";
#else
		return (Config.Debug ? "RelWithDebInfo" : "Release");
#endif
	}
	String Builder::GetGlobalVitexPath()
	{
#if VI_MICROSOFT
		String CacheDirectory = *OS::Directory::GetModule();
		if (CacheDirectory.back() != '/' && CacheDirectory.back() != '\\')
			CacheDirectory += VI_SPLITTER;
		CacheDirectory += ".cache";
		CacheDirectory += VI_SPLITTER;
		CacheDirectory += "vitex";
		return CacheDirectory;
#else
		return "/var/lib/asx/vitex";
#endif

	}
	String Builder::GetBuildingDirectory(EnvironmentConfig& Env, const String& LocalTarget)
	{
		return Env.Registry + ".make";
	}
	String Builder::GetLocalTargetsDirectory(EnvironmentConfig& Env, const String& Name)
	{
		return Stringify::Text("%s%s%cbin", Env.Registry.c_str(), Name.c_str(), VI_SPLITTER);
	}
	String Builder::GetGlobalTargetsDirectory(EnvironmentConfig& Env, const String& Name)
	{
		String Owner = Name.substr(0, Name.find('/'));
		String Repository = OS::Path::GetFilename(Name.c_str());
		String Path = Env.Registry + ".bin";
		Path += VI_SPLITTER;
		Path += Owner;
		return Path;
	}
	String Builder::GetAddonTarget(EnvironmentConfig& Env, const String& Name)
	{
		String Owner = Name.substr(0, Name.find('/'));
		String Repository = OS::Path::GetFilename(Name.c_str());
		String Path = Env.Registry + ".bin";
		Path += VI_SPLITTER;
		Path += Owner;
		Path += VI_SPLITTER;
		Path += Repository;
		return Path;
	}
	String Builder::GetAddonTargetLibrary(EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, bool* IsVM)
	{
		if (IsVM)
			*IsVM = false;

		String BaseName = GetAddonTarget(Env, Name);
		auto Result = OS::Path::Resolve(BaseName.c_str());
		String Path1 = (Result ? *Result : BaseName);
		if (OS::File::IsExists(Path1.c_str()))
			return Path1;

		for (auto& Ext : VM->GetCompileIncludeOptions().Exts)
		{
			String Path = Path1 + Ext;
			if (OS::File::IsExists(Path.c_str()))
				return Path;
		}

		UPtr<Schema> Info = GetAddonInfo(Env, Name);
		if (Info->GetVar("type").GetBlob() != "vm")
			return Path1;

		if (IsVM)
			*IsVM = true;

		String Index = Info->GetVar("index").GetBlob();
		Path1 = Env.Registry + Name + VI_SPLITTER + Index;
		Result = OS::Path::Resolve(Path1.c_str());
		if (Result)
			Path1 = *Result;
		return Path1;
	}
	Schema* Builder::GetAddonInfo(EnvironmentConfig& Env, const String& Name)
	{
		String LocalTarget = Env.Registry + Name + VI_SPLITTER + REPOSITORY_FILE_ADDON;
		auto Data = OS::File::ReadAsString(LocalTarget);
		if (!Data)
			return nullptr;

		auto Result = Schema::FromJSON(*Data);
		return Result ? *Result : nullptr;
	}
	UnorderedMap<String, String> Builder::GetBuildKeys(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings, bool IsAddon)
	{
		String ConfigSettingsArray;
		for (auto& Item : Settings)
		{
			size_t Value = VM->GetProperty((Features)Item.second);
			ConfigSettingsArray += Stringify::Text("{ (uint32_t)%i, (size_t)%" PRIu64 " }, ", Item.second, (uint64_t)Value);
		}

		String ConfigSystemAddonsArray;
		for (auto& Item : VM->GetSystemAddons())
		{
			if (Item.second.Exposed)
				ConfigSystemAddonsArray += Stringify::Text("\"%s\", ", Item.first.c_str());
		}

		String ConfigLibrariesArray, ConfigFunctionsArray;
		for (auto& Item : VM->GetCLibraries())
		{
			ConfigLibrariesArray += Stringify::Text("{ \"%s\", %s }, ", Item.first.c_str(), Item.second.IsAddon ? "true" : "false");
			if (Item.second.IsAddon)
				continue;

			for (auto& Function : Item.second.Functions)
				ConfigFunctionsArray += Stringify::Text("{ \"%s\", { \"%s\", \"%s\" } }, ", Item.first.c_str(), Function.first.c_str());
		}

		String FeatureList;
		auto* Lib = Vitex::Runtime::Get();
		Vector<std::pair<String, bool>> Features =
		{
			{ "ALLOCATOR", Lib->HasAllocator() },
			{ "BINDINGS", Lib->HasBindings() && !IsAddon },
			{ "BACKTRACE", Lib->HasBacktrace() && !IsAddon },
			{ "WEPOLL", Lib->HasWindowsEpoll() && !IsAddon },
			{ "FCTX", Lib->HasFContext() && !IsAddon },
			{ "SIMD", Lib->HasSIMD() && !IsAddon },
			{ "ANGELSCRIPT", Lib->HasAngelScript() && !IsAddon },
			{ "ASSIMP", Lib->HasAssimp() && IsUsingEngine(VM) && !IsAddon },
			{ "FREETYPE", Lib->HasFreeType() && IsUsingGUI(VM) && !IsAddon },
			{ "GLEW", Lib->HasGLEW() && IsUsingGraphics(VM) && !IsAddon },
			{ "OPENAL", Lib->HasOpenAL() && IsUsingAudio(VM) && !IsAddon },
			{ "OPENGL", Lib->HasOpenGL() && IsUsingGraphics(VM) && !IsAddon },
			{ "OPENSSL", Lib->HasOpenSSL() && IsUsingCrypto(VM) && !IsAddon },
			{ "ZLIB", Lib->HasZLib() && IsUsingCompression(VM) && !IsAddon },
			{ "SDL2", Lib->HasSDL2() && IsUsingGraphics(VM) && !IsAddon },
			{ "SQLITE", Lib->HasSQLite() && IsUsingSQLite(VM) && !IsAddon },
			{ "POSTGRESQL", Lib->HasPostgreSQL() && IsUsingPostgreSQL(VM) && !IsAddon },
			{ "MONGOC", Lib->HasMongoDB() && IsUsingMongoDB(VM) && !IsAddon },
			{ "SPIRV", Lib->HasSPIRV() && IsUsingGraphics(VM) && !IsAddon },
			{ "BULLET3", Lib->HasBullet3() && IsUsingPhysics(VM) && !IsAddon },
			{ "RMLUI", Lib->HasRmlUI() && IsUsingGUI(VM) && !IsAddon },
			{ "TINYFILEDIALOGS", Lib->HasTinyFileDialogs() && IsUsingOS(VM) && !IsAddon },
			{ "STB", Lib->HasSTB() && IsUsingEngine(VM) && !IsAddon },
			{ "PUGIXML", Lib->HasPugiXML() && IsUsingEngine(VM) && !IsAddon },
			{ "RAPIDJSON", Lib->HasRapidJSON() && IsUsingEngine(VM) && !IsAddon },
			{ "SHADERS", Lib->HasShaders() && IsUsingGraphics(VM) && !IsAddon }
		};

		for (auto& Item : Features)
			FeatureList += Stringify::Text("set(VI_%s %s CACHE BOOL \"-\")\n", Item.first.c_str(), Item.second ? "ON" : "OFF");

		if (!FeatureList.empty())
			FeatureList.erase(FeatureList.end() - 1);

		Schema* ConfigInstallArray = Var::Set::Array();
		if (Lib->HasSPIRV() && IsUsingGraphics(VM) && !IsAddon)
		{
			ConfigInstallArray->Push(Var::String("spirv-cross"));
			ConfigInstallArray->Push(Var::String("glslang"));
		}
		if (Lib->HasZLib() && IsUsingCompression(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("zlib"));
		if (Lib->HasAssimp() && IsUsingEngine(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("assimp"));
		if (Lib->HasFreeType() && IsUsingGUI(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("freetype"));
		if (Lib->HasSDL2() && IsUsingGraphics(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("sdl2"));
		if (Lib->HasOpenAL() && IsUsingAudio(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("openal-soft"));
		if (Lib->HasGLEW() && IsUsingGraphics(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("glew"));
		if (Lib->HasOpenSSL() && IsUsingCrypto(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("openssl"));
		if (Lib->HasMongoDB() && IsUsingMongoDB(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("mongo-c-driver"));
		if (Lib->HasPostgreSQL() && IsUsingPostgreSQL(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("libpq"));
		if (Lib->HasSQLite() && IsUsingSQLite(VM) && !IsAddon)
			ConfigInstallArray->Push(Var::String("sqlite3"));

		String VitexPath = GetGlobalVitexPath();
		Stringify::Replace(VitexPath, '\\', '/');

		UnorderedMap<String, String> Keys;
		Keys["BUILDER_CONFIG_INSTALL"] = Schema::ToJSON(ConfigInstallArray);
		Keys["BUILDER_CONFIG_SETTINGS"] = ConfigSettingsArray;
		Keys["BUILDER_CONFIG_LIBRARIES"] = ConfigLibrariesArray;
		Keys["BUILDER_CONFIG_FUNCTIONS"] = ConfigFunctionsArray;
		Keys["BUILDER_CONFIG_ADDONS"] = ConfigSystemAddonsArray;
		Keys["BUILDER_CONFIG_TS_IMPORTS"] = Config.TsImports ? "true" : "false";
		Keys["BUILDER_CONFIG_SYSTEM_ADDONS"] = Config.Addons ? "true" : "false";
		Keys["BUILDER_CONFIG_CLIBRARIES"] = Config.CLibraries ? "true" : "false";
		Keys["BUILDER_CONFIG_CFUNCTIONS"] = Config.CFunctions ? "true" : "false";
		Keys["BUILDER_CONFIG_FILES"] = Config.Files ? "true" : "false";
		Keys["BUILDER_CONFIG_REMOTES"] = Config.Remotes ? "true" : "false";
		Keys["BUILDER_CONFIG_TRANSLATOR"] = Config.Translator ? "true" : "false";
		Keys["BUILDER_CONFIG_ESSENTIALS_ONLY"] = Config.EssentialsOnly ? "true" : "false";
		Keys["BUILDER_VITEX_URL"] = ConfigSystemAddonsArray;
		Keys["BUILDER_VITEX_PATH"] = VitexPath;
		Keys["BUILDER_FEATURES"] = FeatureList;
		Keys["BUILDER_VERSION"] = GetSystemVersion();
		Keys["BUILDER_MODE"] = Env.Mode;
		Keys["BUILDER_OUTPUT"] = Env.Name.empty() ? "build_target" : Env.Name;
		return Keys;
	}

	Option<String> Templates::Fetch(const UnorderedMap<String, String>& Keys, const String& Path)
	{
		if (!Files)
		{
#ifdef HAS_CODE_BUNDLE
			using FilesType = UnorderedMap<String, String>;
			Files = VI_NEW(FilesType);
			code_bundle::foreach(nullptr, [](void*, const char* Path, const char* File, unsigned int FileSize)
			{
				Files->insert(std::make_pair(String(Path), String(File, FileSize)));
			});
#else
			return Optional::None;
#endif
		}

		auto It = Files->find(Path);
		if (It == Files->end())
			return Optional::None;

		String Result = It->second;
		for (auto& Value : Keys)
			Stringify::Replace(Result, "{{" + Value.first + "}}", Value.second);
		return Result;
	}
	void Templates::Cleanup()
	{
		VI_DELETE(unordered_map, Files);
		Files = nullptr;
	}
	UnorderedMap<String, String>* Templates::Files = nullptr;
}