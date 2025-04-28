#ifndef APP_H
#define APP_H
#include "builder.h"
#include <vengeance/bindings.h>
#include <vitex/network.h>

using namespace vitex::layer;
using namespace vitex::network;

namespace asx
{
	typedef std::function<int(const std::string_view&)> command_callback;

	struct environment_command
	{
		command_callback callback;
		vector<string> arguments;
		string description;
	};

	class environment : public singleton<environment>
	{
	public:
		unordered_map<string, vector<environment_command>> commands;
		unordered_map<string, uint32_t> settings;
		unordered_set<string> flags;
		environment_config env;
		program_entrypoint entrypoint;
		system_config config;
		event_loop* loop;
		virtual_machine* vm;
		immediate_context* context;
		compiler* unit;
		std::mutex mutex;

	public:
		environment(int args_count, char** args);
		~environment();
		int dispatch();
		void shutdown(int value);
		void interrupt(int value);
		void abort(const char* signal);
		size_t get_init_flags();

	private:
		void add_default_commands();
		void add_default_settings();
		void add_command(const std::string_view& category, const std::string_view& name, const std::string_view& description, bool is_flag_only, const command_callback& callback);
		exit_status execute_argument(const unordered_set<string>& names);
		environment_command* find_argument(const std::string_view& name);
		void print_introduction(const char* label);
		void print_help();
		void print_properties();
		void print_dependencies();
		void listen_for_signals();
		static void exit_process(exit_status code);
		expects_preprocessor<include_type> import_addon(preprocessor* base, const include_result& file, string& output);
	};
}
#endif