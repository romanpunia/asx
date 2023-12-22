import from
{
    "console",
    "buffers"
};

#pragma cimport("kernel32.dll", "CreateToolhelp32Snapshot", "uptr@ __CreateToolhelp32Snapshot(uint32, uint32)")
#pragma cimport("kernel32.dll", "Process32First", "int32 __Process32First(uptr@, uptr@)")
#pragma cimport("kernel32.dll", "Process32Next", "int32 __Process32Next(uptr@, uptr@)")
#pragma cimport("kernel32.dll", "GetCurrentProcessId", "uint32 __GetCurrentProcessId()")
#pragma cimport("kernel32.dll", "CloseHandle", "int32 __CloseHandle(uptr@)")
#define MAX_PATH 260
#define TH32CS_SNAPPROCESS 0x00000002
#define TH32CS_SNAPMODULE 0x00000008
#define offsetof_PROCESSENTRY32_dwSize 0
#define offsetof_PROCESSENTRY32_th32ProcessID 8
#define offsetof_PROCESSENTRY32_szExeFile 44
#ifdef SOF_CreateToolhelp32Snapshot
#define CreateToolhelp32Snapshot(flags, pid) __CreateToolhelp32Snapshot(flags, pid)
#else
#define CreateToolhelp32Snapshot(_, _) null
#endif
#ifdef SOF_Process32First
#define Process32First(snapshot, process) __Process32First(snapshot, process)
#else
#define Process32First(_, _) 0
#endif
#ifdef SOF_Process32Next
#define Process32Next(snapshot, process) __Process32Next(snapshot, process)
#else
#define Process32Next(_, _) 0
#endif
#ifdef SOF_CloseHandle
#define GetCurrentProcessId() __GetCurrentProcessId()
#else
#define GetCurrentProcessId() 0
#endif
#ifdef SOF_CloseHandle
#define CloseHandle(handle) __CloseHandle(handle)
#else
#define CloseHandle(_) 0
#endif

/*
    I recommend running following commands:
        1. vi -d -f examples/processes
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