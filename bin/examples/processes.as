#include <std/console.as>
#include <std/buffers.as>
#include "utils/win32.as"

/*
    I recommend running following commands:
        1. vi -d -a -f examples/processes
        2. i c
    to see what code has been generated
*/
int main()
{
    uptr@ handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPMODULE, 0); 
    if (handle is null)
    {
        console@ output = console::get();
        output.write_line("cannot fetch process list handle");
        return 1;
    }

    console@ output = console::get();
    output.write_line("currently running processes:");
    
    char_buffer@ buffer = char_buffer(1024);
    buffer.store(offsetof_PROCESSENTRY32_dwSize, int32(buffer.size())); // store int32: PROCESSENTRY32::dwSize

    uptr@ process = buffer.get_ptr();
    Process32First(@handle, @process);

    uint32 current_process_id = GetCurrentProcessId();
    do
    {
        uint32 process_id = 0;
        if (!buffer.load(offsetof_PROCESSENTRY32_th32ProcessID, process_id)) // load int32: PROCESSENTRY32::th32ProcessID
            break;

        string process_name;
        if (!buffer.interpret(offsetof_PROCESSENTRY32_szExeFile, process_name, MAX_PATH)) // load char array from pointer: PROCESSENTRY32::szExeFile
            break;

        if (process_id == current_process_id)
            output.write_line("> [pid:" + to_string(process_id) + "] " + process_name);
        else
            output.write_line("  [pid:" + to_string(process_id) + "] " + process_name);
    } while (Process32Next(@handle, @process) > 0);

    CloseHandle(@handle);
    return 0;
}