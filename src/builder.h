#ifndef BUILDER_H
#define BUILDER_H
#include "runtime.hpp"
#include <vengeance/vengeance.h>

namespace ASX
{
	enum class StatusCode
	{
		OK = 0,
		CommandError = 1,
		CommandNotFound = 2,
		GenerationError = 3,
		ByteCodeError = 4,
		DependencyError = 5,
		ConfigurationError = 6,
		BuildError = 7
	};

	class Builder
	{
	public:
		static StatusCode CompileIntoAddon(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const std::string_view& Name, String& Output);
		static StatusCode ImportIntoAddon(EnvironmentConfig& Env, VirtualMachine* VM, const std::string_view& Name, String& Output);
		static StatusCode InitializeIntoAddon(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings);
		static StatusCode PullAddonRepository(SystemConfig& Config, EnvironmentConfig& Env);
		static StatusCode CompileIntoExecutable(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings);
		static UnorderedMap<String, uint32_t> GetDefaultSettings();
		static String GetSystemVersion();
		static String GetAddonTargetLibrary(EnvironmentConfig& Env, VirtualMachine* VM, const std::string_view& Name, bool* IsVM);
		static bool IsAddonTargetExists(EnvironmentConfig& Env, VirtualMachine* VM, const std::string_view& Name, bool Nested = false);

	private:
		static StatusCode ExecuteGit(SystemConfig& Config, const std::string_view& Command);
		static StatusCode ExecuteCMake(SystemConfig& Config, const std::string_view& Command);
		static bool ExecuteCommand(SystemConfig& Config, const std::string_view& Label, const std::string_view& Command, int SuccessExitCode);
		static bool AppendTemplate(const UnorderedMap<String, String>& Keys, const std::string_view& TargetPath, const std::string_view& TemplatePath);
		static bool AppendByteCode(SystemConfig& Config, EnvironmentConfig& Env, const std::string_view& Path);
		static bool AppendDependencies(EnvironmentConfig& Env, VirtualMachine* VM, const std::string_view& TargetDirectory);
		static bool AppendVitex(SystemConfig& Config);
		static bool IsDirectoryEmpty(const std::string_view& Target);
		static const char* GetBuildType(SystemConfig& Config);
		static String GetGlobalVitexPath();
		static String GetBuildingDirectory(EnvironmentConfig& Env, const std::string_view& LocalTarget);
		static String GetGlobalTargetsDirectory(EnvironmentConfig& Env, const std::string_view& Name);
		static String GetLocalTargetsDirectory(EnvironmentConfig& Env, const std::string_view& Name);
		static String GetAddonTarget(EnvironmentConfig& Env, const std::string_view& Name);
		static Schema* GetAddonInfo(EnvironmentConfig& Env, const std::string_view& Name);
		static UnorderedMap<String, String> GetBuildKeys(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings, bool IsAddon);
	};

	class Control
	{
	public:
		static bool Has(SystemConfig& Config, AccessOption Option);
	};

	class Templates
	{
	private:
		static UnorderedMap<String, String>* Files;

	public:
		static Option<String> Fetch(const UnorderedMap<String, String>& Keys, const std::string_view& Path);
		static void Cleanup();
	};
}
#endif