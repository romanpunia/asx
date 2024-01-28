#ifndef APP_H
#define APP_H
#include "builder.h"
#include <vitex/core/bindings.h>
#include <vitex/core/network.h>

using namespace Vitex::Engine;
using namespace Vitex::Network;

namespace ASX
{
	typedef std::function<int(const String&)> CommandCallback;

	struct EnvironmentCommand
	{
		CommandCallback Callback;
		String Description;
	};

	class Environment
	{
	private:
		UnorderedMap<String, UnorderedMap<String, EnvironmentCommand>> Commands;
		UnorderedMap<String, uint32_t> Settings;
		UnorderedSet<String> Flags;
		EnvironmentConfig Env;
		ProgramEntrypoint Entrypoint;
		SystemConfig Config;
		EventLoop* Loop;
		VirtualMachine* VM;
		ImmediateContext* Context;
		Compiler* Unit;

	public:
		Environment(int ArgsCount, char** Args);
		~Environment();
		int Dispatch();
		void Shutdown(int Value);
		void Interrupt(int Value);
		void Abort(const char* Signal);
		size_t GetInitFlags();

	private:
		void AddDefaultCommands();
		void AddDefaultSettings();
		void AddCommand(const String& Category, const String& Name, const String& Description, bool IsFlagOnly, const CommandCallback& Callback);
		void PrintIntroduction(const char* Label);
		void PrintHelp();
		void PrintProperties();
		void PrintDependencies();
		void ListenForSignals();
		ExpectsPreprocessor<IncludeType> ImportAddon(Preprocessor* Base, const IncludeResult& File, String& Output);
	};
}
#endif