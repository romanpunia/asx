#ifndef APP_H
#define APP_H
#include "builder.h"
#include <vitex/bindings.h>
#include <vitex/network.h>

using namespace Vitex::Engine;
using namespace Vitex::Network;

namespace ASX
{
	typedef std::function<int(const std::string_view&)> CommandCallback;

	struct EnvironmentCommand
	{
		CommandCallback Callback;
		Vector<String> Arguments;
		String Description;
	};

	class Environment
	{
	private:
		UnorderedMap<String, Vector<EnvironmentCommand>> Commands;
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
		void AddCommand(const std::string_view& Category, const std::string_view& Name, const std::string_view& Description, bool IsFlagOnly, const CommandCallback& Callback);
		ExitStatus ExecuteArgument(const UnorderedSet<String>& Names);
		EnvironmentCommand* FindArgument(const std::string_view& Name);
		void PrintIntroduction(const char* Label);
		void PrintHelp();
		void PrintProperties();
		void PrintDependencies();
		void ListenForSignals();
		static void ExitProcess(ExitStatus Code);
		ExpectsPreprocessor<IncludeType> ImportAddon(Preprocessor* Base, const IncludeResult& File, String& Output);
	};
}
#endif