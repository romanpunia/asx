#ifndef BUILDER_H
#define BUILDER_H
#include "runtime.hpp"
#include <vitex/vitex.h>

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
		static StatusCode CompileIntoAddon(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, String& Output);
		static StatusCode ImportIntoAddon(EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, String& Output);
		static StatusCode InitializeIntoAddon(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings);
		static StatusCode PullAddonRepository(EnvironmentConfig& Env);
		static StatusCode CompileIntoExecutable(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings);
		static UnorderedMap<String, uint32_t> GetDefaultSettings();
		static String GetSystemVersion();
		static String GetAddonTargetLibrary(EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, bool* IsVM);
		static bool IsAddonTargetExists(EnvironmentConfig& Env, VirtualMachine* VM, const String& Name, bool Nested = false);

	private:
		static StatusCode ExecuteGit(const String& Command);
		static StatusCode ExecuteCMake(const String& Command);
		static bool ExecuteCommand(const String& Label, const String& Command, int SuccessExitCode);
		static bool AppendTemplate(const UnorderedMap<String, String>& Keys, const String& TargetPath, const String& TemplatePath);
		static bool AppendByteCode(SystemConfig& Config, EnvironmentConfig& Env, const String& Path);
		static bool AppendDependencies(EnvironmentConfig& Env, VirtualMachine* VM, const String& TargetDirectory);
		static bool AppendVitex(const String& TargetPath);
		static bool IsUsingCompression(VirtualMachine* VM);
		static bool IsUsingSchemas(VirtualMachine* VM);
		static bool IsUsingCrypto(VirtualMachine* VM);
		static bool IsUsingAudio(VirtualMachine* VM);
		static bool IsUsingGraphics(VirtualMachine* VM);
		static bool IsUsingEngine(VirtualMachine* VM);
		static bool IsUsingSQLite(VirtualMachine* VM);
		static bool IsUsingPostgreSQL(VirtualMachine* VM);
		static bool IsUsingMongoDB(VirtualMachine* VM);
		static bool IsUsingPhysics(VirtualMachine* VM);
		static bool IsUsingGUI(VirtualMachine* VM);
		static bool IsUsingOS(VirtualMachine* VM);
		static bool IsDirectoryEmpty(const String& Target);
		static const char* GetBuildType(SystemConfig& Config);
		static String GetGlobalVitexPath();
		static String GetBuildingDirectory(EnvironmentConfig& Env, const String& LocalTarget);
		static String GetGlobalTargetsDirectory(EnvironmentConfig& Env, const String& Name);
		static String GetLocalTargetsDirectory(EnvironmentConfig& Env, const String& Name);
		static String GetAddonTarget(EnvironmentConfig& Env, const String& Name);
		static Schema* GetAddonInfo(EnvironmentConfig& Env, const String& Name);
		static UnorderedMap<String, String> GetBuildKeys(SystemConfig& Config, EnvironmentConfig& Env, VirtualMachine* VM, const UnorderedMap<String, uint32_t>& Settings, bool IsAddon);
	};

	class Templates
	{
	private:
		static UnorderedMap<String, String>* Files;

	public:
		static Option<String> Fetch(const UnorderedMap<String, String>& Keys, const String& Path);
		static void Cleanup();
	};
}
#endif